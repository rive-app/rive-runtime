#include "rive/artboard.hpp"
#include <map>
#include "rive/core/vector_binary_writer.hpp"
#include "rive/file.hpp"
#include "rive/generated/artboard_base.hpp"
#include "rive/generated/backboard_base.hpp"
#include "rive/generated/component_base.hpp"
#include "rive/generated/layout_component_base.hpp"
#include "rive/generated/semantic/semantic_data_base.hpp"
#include "rive/semantic/semantic_data.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>

// A minimal .riv assembled by hand, so a single property can be placed where
// no exporter would put it.
namespace
{
class RivBytes
{
public:
    RivBytes() : m_writer(&m_objects) {}

    void object(uint16_t typeKey) { m_writer.writeVarUint((uint64_t)typeKey); }
    void end() { m_writer.writeVarUint((uint64_t)0); }

    // ToC field ids: 0 uint (bools ride as uints), 1 string, 2 double.
    void propUint(uint16_t key, uint64_t value)
    {
        m_toc[key] = 0;
        m_writer.writeVarUint((uint64_t)key);
        m_writer.writeVarUint(value);
    }
    void propBool(uint16_t key, bool value)
    {
        m_toc[key] = 0;
        m_writer.writeVarUint((uint64_t)key);
        m_writer.write((uint8_t)(value ? 1 : 0));
    }
    void propString(uint16_t key, const std::string& value)
    {
        m_toc[key] = 1;
        m_writer.writeVarUint((uint64_t)key);
        m_writer.write(value);
    }
    void propFloat(uint16_t key, float value)
    {
        m_toc[key] = 2;
        m_writer.writeVarUint((uint64_t)key);
        m_writer.write(value);
    }
    // A key the file's ToC does not describe.
    void propUintOutsideToc(uint16_t key, uint64_t value)
    {
        m_writer.writeVarUint((uint64_t)key);
        m_writer.writeVarUint(value);
    }

    std::vector<uint8_t> bytes() const
    {
        std::vector<uint8_t> out;
        rive::VectorBinaryWriter writer(&out);
        writer.write((const uint8_t*)"RIVE", 4);
        writer.writeVarUint((uint64_t)rive::File::majorVersion);
        writer.writeVarUint((uint64_t)rive::File::minorVersion);
        writer.writeVarUint((uint64_t)0); // file id
        for (const auto& entry : m_toc)
        {
            writer.writeVarUint((uint64_t)entry.first);
        }
        writer.writeVarUint((uint64_t)0);
        // The reader refreshes its packed int every 8 bits: one uint32 per
        // four keys (RuntimeHeader::read).
        uint32_t packed = 0;
        int bit = 0;
        for (const auto& entry : m_toc)
        {
            packed |= entry.second << bit;
            bit += 2;
            if (bit == 8)
            {
                writer.write(packed);
                packed = 0;
                bit = 0;
            }
        }
        if (bit != 0)
        {
            writer.write(packed);
        }
        out.insert(out.end(), m_objects.begin(), m_objects.end());
        return out;
    }

private:
    std::vector<uint8_t> m_objects;
    rive::VectorBinaryWriter m_writer;
    std::map<uint16_t, uint32_t> m_toc;
};

void writeArtboard(RivBytes& riv)
{
    riv.object(rive::BackboardBase::typeKey);
    riv.end();
    riv.object(rive::ArtboardBase::typeKey);
    riv.propString(rive::ComponentBase::namePropertyKey, "A");
    riv.propFloat(rive::LayoutComponentBase::widthPropertyKey, 100.0f);
    riv.propFloat(rive::LayoutComponentBase::heightPropertyKey, 100.0f);
    riv.end();
}
} // namespace

// isHidden and its siblings are bits of stateFlags: the registry types them
// as bool, but SemanticData stores no such field and rejects the key. The
// skip switch had no bool case, so the value byte was read as the next
// property key and the rest of the stream was misparsed while the import
// still reported success.
TEST_CASE("an unknown bool property is skipped, not misread as a key",
          "[file][malformed]")
{
    RivBytes riv;
    writeArtboard(riv);
    riv.object(rive::SemanticDataBase::typeKey);
    riv.propUint(rive::ComponentBase::parentIdPropertyKey, 0);
    riv.propBool(rive::SemanticDataBase::isHiddenPropertyKey, true);
    riv.propString(rive::SemanticDataBase::labelPropertyKey, "after");
    riv.end();

    rive::ImportResult result;
    auto file = rive::File::import(riv.bytes(), &gNoOpFactory, &result);
    REQUIRE(result == rive::ImportResult::success);
    REQUIRE(file != nullptr);

    rive::SemanticData* semantic = nullptr;
    for (auto* object : file->artboard()->objects())
    {
        if (object != nullptr && object->is<rive::SemanticData>())
        {
            semantic = object->as<rive::SemanticData>();
        }
    }
    REQUIRE(semantic != nullptr);
    // The property after the skipped bool still lands.
    CHECK(semantic->label() == "after");
    // Skipped, not applied: an object-level flag key is not a packed field.
    CHECK(semantic->stateFlags() == 0);
}

// A key that neither the runtime nor the ToC can type leaves the stream
// unreadable. Dropping the object and carrying on read the bytes that
// followed as objects, which loaded a file that was garbage past that point.
TEST_CASE("a property missing from the ToC fails the import",
          "[file][malformed]")
{
    RivBytes riv;
    riv.object(rive::BackboardBase::typeKey);
    riv.end();
    riv.object(rive::ArtboardBase::typeKey);
    riv.propString(rive::ComponentBase::namePropertyKey, "A");
    riv.propUintOutsideToc(65000, 1);
    riv.propFloat(rive::LayoutComponentBase::widthPropertyKey, 100.0f);
    riv.end();

    rive::ImportResult result;
    auto file = rive::File::import(riv.bytes(), &gNoOpFactory, &result);
    CHECK(result == rive::ImportResult::malformed);
    CHECK(file == nullptr);
}

// Stripping assets walks the same object stream and used to carry on past
// the desync as well.
TEST_CASE("stripping assets from a file with an untyped property fails",
          "[file][malformed]")
{
    RivBytes riv;
    riv.object(rive::BackboardBase::typeKey);
    riv.end();
    riv.object(rive::ArtboardBase::typeKey);
    riv.propString(rive::ComponentBase::namePropertyKey, "A");
    riv.propUintOutsideToc(65000, 1);
    riv.end();

    rive::ImportResult result = rive::ImportResult::success;
    auto stripped = rive::File::stripAssets(riv.bytes(), {}, &result);
    CHECK(result == rive::ImportResult::malformed);
    CHECK(stripped.empty());
}

// An object type this runtime does not know is the forward-compatibility
// path and stays benign: its ToC-typed properties are skipped and the file
// loads without it.
TEST_CASE("an unknown object type is skipped and the file still loads",
          "[file][malformed]")
{
    RivBytes riv;
    writeArtboard(riv);
    riv.object(65001);
    riv.propUint(65002, 7);
    riv.end();

    rive::ImportResult result;
    auto file = rive::File::import(riv.bytes(), &gNoOpFactory, &result);
    REQUIRE(result == rive::ImportResult::success);
    REQUIRE(file != nullptr);
    CHECK(file->artboard()->name() == "A");
}

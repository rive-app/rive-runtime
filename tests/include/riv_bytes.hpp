#ifndef _RIVE_TESTS_RIV_BYTES_HPP_
#define _RIVE_TESTS_RIV_BYTES_HPP_

#include "rive/core/vector_binary_writer.hpp"
#include "rive/file.hpp"
#include "rive/generated/artboard_base.hpp"
#include "rive/generated/backboard_base.hpp"
#include "rive/generated/component_base.hpp"
#include "rive/generated/layout_component_base.hpp"

#include <map>
#include <string>
#include <vector>

// A minimal .riv assembled by hand, so an object or a property can be placed
// where no exporter would put it -- which is how the import paths that only
// a malformed or forward-dated file reaches get exercised.
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

// One 100x100 artboard named "A", with no Backboard of its own -- for streams
// that need to place something between the two.
inline void writeArtboardObject(RivBytes& riv)
{
    riv.object(rive::ArtboardBase::typeKey);
    riv.propString(rive::ComponentBase::namePropertyKey, "A");
    riv.propFloat(rive::LayoutComponentBase::widthPropertyKey, 100.0f);
    riv.propFloat(rive::LayoutComponentBase::heightPropertyKey, 100.0f);
    riv.end();
}

// The smallest loadable file: a Backboard and one 100x100 artboard named "A".
inline void writeArtboard(RivBytes& riv)
{
    riv.object(rive::BackboardBase::typeKey);
    riv.end();
    writeArtboardObject(riv);
}

#endif

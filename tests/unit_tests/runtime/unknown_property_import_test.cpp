#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/generated/artboard_base.hpp"
#include "rive/generated/backboard_base.hpp"
#include "rive/generated/component_base.hpp"
#include "rive/generated/layout_component_base.hpp"
#include "rive/generated/semantic/semantic_data_base.hpp"
#include "rive/semantic/semantic_data.hpp"
#include "riv_bytes.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>

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

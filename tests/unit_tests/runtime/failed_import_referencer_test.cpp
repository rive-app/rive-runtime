#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/generated/backboard_base.hpp"
#include "rive/generated/component_base.hpp"
#include "rive/generated/data_bind/converters/data_converter_group_item_base.hpp"
#include "rive/generated/data_bind/converters/data_converter_rounder_base.hpp"
#include "rive/generated/nested_artboard_base.hpp"
#include "riv_bytes.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>

// Several objects hand themselves to the BackboardImporter early in import()
// and can still fail afterwards: NestedArtboard and ScriptInputArtboard join
// its referencer list, DataBind, DataConverter, DataConverterGroupItem and
// KeyFrameInterpolator join one of the others. All of those lists hold raw
// pointers that nothing walks until resolve(), which runs last -- the
// BackboardImporter is pushed first and ImportStack::resolve() reverse
// iterates. Freeing a failed object the moment it fails therefore left
// resolve() reading a block that later allocations had already taken over.
//
// Observed as a fatal EXC_BAD_ACCESS at 0x18 inside
// BackboardImporter::resolve(): the load of referencedArtboardId() out of the
// referencer's vtable, with the object pointer intact and its vtable pointer
// reading back as zero.
//
// The sharpest assertion here is the sanitizer -- `./test.sh asan` traps the
// use-after-free precisely. Without it these still stand as behaviour tests:
// an object whose import fails is skipped and the rest of the file loads.

// The Sentry crash itself. A NestedArtboard ahead of every Artboard has no
// ArtboardImporter to attach to, so Component::import fails it -- but it has
// already registered as an ArtboardReferencer by then.
TEST_CASE("a nested artboard that fails to import does not outlive itself",
          "[file][malformed]")
{
    RivBytes riv;
    riv.object(rive::BackboardBase::typeKey);
    riv.end();
    // No artboard is open yet, so this one cannot import.
    riv.object(rive::NestedArtboardBase::typeKey);
    riv.propUint(rive::NestedArtboardBase::artboardIdPropertyKey, 0);
    riv.end();
    writeArtboardObject(riv);

    rive::ImportResult result;
    auto file = rive::File::import(riv.bytes(), &gNoOpFactory, &result);
    REQUIRE(result == rive::ImportResult::success);
    REQUIRE(file != nullptr);
    // The artboard that follows the dropped object still loads.
    REQUIRE(file->artboard() != nullptr);
    CHECK(file->artboard()->name() == "A");
}

// The same hazard reached through a different list, so it stays covered even
// if the referencer registrations move again. A DataConverterGroupItem with
// no DataConverterGroup open fails after joining
// m_DataConverterGroupItemReferencers, and resolve() writes a converter into
// every entry of that list.
TEST_CASE("a converter group item that fails to import does not outlive "
          "itself",
          "[file][malformed]")
{
    RivBytes riv;
    riv.object(rive::BackboardBase::typeKey);
    riv.end();
    // Lands in the importer's converter list, so the group item below gets
    // far enough to be written to rather than skipped on a bounds check.
    riv.object(rive::DataConverterRounderBase::typeKey);
    riv.end();
    // No group is open, so this one cannot import.
    riv.object(rive::DataConverterGroupItemBase::typeKey);
    riv.propUint(rive::DataConverterGroupItemBase::converterIdPropertyKey, 0);
    riv.end();
    writeArtboardObject(riv);

    rive::ImportResult result;
    auto file = rive::File::import(riv.bytes(), &gNoOpFactory, &result);
    REQUIRE(result == rive::ImportResult::success);
    REQUIRE(file != nullptr);
    REQUIRE(file->artboard() != nullptr);
    CHECK(file->artboard()->name() == "A");
}

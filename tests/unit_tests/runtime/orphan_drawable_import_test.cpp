#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/generated/component_base.hpp"
#include "rive/generated/node_base.hpp"
#include "rive/generated/shapes/shape_base.hpp"
#include "riv_bytes.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>

// Artboard::validateObjects() only parks invalid objects in a cycle where some
// object's validity changed, and every slot starts out "invalid". An artboard
// whose objects are all invalid therefore never changes on the first cycle and
// keeps every orphan, and Component::onAddedDirty then dereferences the
// missing parent (Sentry RIVE_NATIVE-141: EXC_BAD_ACCESS at 0x0 in
// Drawable::onAddedDirty, parentId 121).

// Control: one valid sibling makes the first cycle "change", so the orphan is
// parked and the file loads.
TEST_CASE("an orphan drawable next to a valid node is dropped",
          "[file][malformed]")
{
    RivBytes riv;
    writeArtboard(riv);
    riv.object(rive::NodeBase::typeKey);
    riv.propUint(rive::ComponentBase::parentIdPropertyKey, 0);
    riv.end();
    riv.object(rive::ShapeBase::typeKey);
    riv.propUint(rive::ComponentBase::parentIdPropertyKey, 121);
    riv.end();

    rive::ImportResult result;
    auto file = rive::File::import(riv.bytes(), &gNoOpFactory, &result);
    REQUIRE(result == rive::ImportResult::success);
    REQUIRE(file != nullptr);
    REQUIRE(file->artboard() != nullptr);
}

// The crash: the orphan is the artboard's only object.
TEST_CASE("an artboard holding only an orphan drawable loads",
          "[file][malformed]")
{
    RivBytes riv;
    writeArtboard(riv);
    riv.object(rive::ShapeBase::typeKey);
    riv.propUint(rive::ComponentBase::parentIdPropertyKey, 121);
    riv.end();

    rive::ImportResult result;
    auto file = rive::File::import(riv.bytes(), &gNoOpFactory, &result);
    REQUIRE(result == rive::ImportResult::success);
    REQUIRE(file != nullptr);
    REQUIRE(file->artboard() != nullptr);
}

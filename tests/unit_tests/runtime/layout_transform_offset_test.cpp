/*
 * Copyright 2026 Rive
 */

// Older editors let x/y be written on a LayoutComponent but never read them,
// so real files carry stale values (db_health_tracker.riv has 8 layouts at
// 36,36). Import gates the composition on riv 7.3, so those render unchanged.
// The two assets below differ only in the version stamp.

#include "rive/artboard.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/layout_component.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <catch.hpp>
#include <vector>

// From gen_layout_fixtures.py: a 200x200 flex row whose 50x50 child sits at
// (0,0) and carries x=30, y=12, so its world position is the offset itself.
static rive::LayoutComponent* offsetChild(rive::Artboard* artboard)
{
    for (auto layout : artboard->find<rive::LayoutComponent>())
    {
        if (layout->is<rive::Artboard>())
        {
            continue;
        }
        // The row is 200 wide; the offset child is the 50x50 one.
        if (layout->layoutWidth() == 50.0f)
        {
            return layout;
        }
    }
    return nullptr;
}

TEST_CASE("a layout's x/y offsets its solved slot", "[layout]")
{
    auto file = ReadRiveFile("assets/layout/transform_offset.riv");
    auto artboard = file->artboard();
    REQUIRE(artboard != nullptr);
    artboard->advance(0.0f);

    auto* child = offsetChild(artboard);
    REQUIRE(child != nullptr);

    // x/y does not feed back into the layout solve.
    REQUIRE(child->layoutX() == 0.0f);
    REQUIRE(child->layoutY() == 0.0f);

    // ...but the world transform carries the offset.
    auto world = child->worldTransform();
    REQUIRE(world[4] == Approx(30.0f));
    REQUIRE(world[5] == Approx(12.0f));
}

TEST_CASE("a layout's x/y round trips through the core property", "[layout]")
{
    auto file = ReadRiveFile("assets/layout/transform_offset.riv");
    auto artboard = file->artboard();
    REQUIRE(artboard != nullptr);
    artboard->advance(0.0f);

    auto* child = offsetChild(artboard);
    REQUIRE(child != nullptr);

    // CoreRegistry calls the virtual getter, so an override returning the
    // laid-out position made the inspector, keying, data binding and scripting
    // all report the wrong value.
    REQUIRE(child->x() == Approx(30.0f));
    REQUIRE(child->y() == Approx(12.0f));
    REQUIRE(
        rive::CoreRegistry::getDouble(child, rive::NodeBase::xPropertyKey) ==
        Approx(30.0f));

    child->x(45.0f);
    REQUIRE(child->x() == Approx(45.0f));

    // Still reachable under its own name, and absent from transform(), which
    // holds only our own offset.
    REQUIRE(child->layoutX() == 0.0f);
    artboard->advance(0.0f);
    REQUIRE(child->transform()[4] == Approx(45.0f));
    REQUIRE(child->transform()[5] == Approx(12.0f));

    // composedTranslation is what a constraint's Offset preserves: our stored
    // x/y alone, never where the layout put us.
    REQUIRE(child->composedTranslation().x == Approx(45.0f));
    REQUIRE(child->composedTranslation().y == Approx(12.0f));

    // computedLocalX reports the origin, which this fixture lays out at (0,0)
    // with no ComponentOrigin, so it lands on the offset too. Protected; read
    // it the way the inspector does.
    REQUIRE(rive::CoreRegistry::getDouble(
                child,
                rive::NodeBase::computedLocalXPropertyKey) == Approx(45.0f));
}

TEST_CASE("a layout's origin leaves its box alone", "[layout]")
{
    auto file = ReadRiveFile("assets/layout/transform_offset.riv");
    auto artboard = file->artboard();
    REQUIRE(artboard != nullptr);
    artboard->advance(0.0f);

    auto* child = offsetChild(artboard);
    REQUIRE(child != nullptr);

    // The box always starts at local zero, so contents never compensate.
    REQUIRE(child->localBounds().left() == 0.0f);
    REQUIRE(child->worldBounds().left() == Approx(child->worldTransform()[4]));

    // No ComponentOrigin on this fixture, so there is no anchor either.
    REQUIRE(child->originOffset().x == 0.0f);
    REQUIRE(child->localAnchor().x == 0.0f);

    // An artboard draws about its origin, so it anchors at zero.
    REQUIRE(artboard->pivotOriginX() == Approx(artboard->originX()));
    REQUIRE(artboard->localAnchor().x == 0.0f);
}

TEST_CASE("a pre-7.3 file ignores a layout's stored x/y", "[layout]")
{
    auto file = ReadRiveFile("assets/layout/transform_offset_legacy.riv");
    auto artboard = file->artboard();
    REQUIRE(artboard != nullptr);
    artboard->advance(0.0f);

    auto* child = offsetChild(artboard);
    REQUIRE(child != nullptr);

    // Same stored x/y as the 7.3 asset, but predates the composition.
    REQUIRE(child->layoutX() == 0.0f);
    auto world = child->worldTransform();
    REQUIRE(world[4] == Approx(0.0f));
    REQUIRE(world[5] == Approx(0.0f));

    // ...so a constraint's Offset can't pick the stale value up either; it
    // keeps reporting the solved position these files always used.
    REQUIRE(child->composedTranslation().x != Approx(30.0f));
    REQUIRE(child->composedTranslation().y != Approx(12.0f));
}

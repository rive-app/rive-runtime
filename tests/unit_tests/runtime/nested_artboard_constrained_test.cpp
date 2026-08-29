/*
 * Copyright 2026 Rive
 */

// A NestedArtboardLayout carries a placement its own transform does not:
//
//   parentWorld * translate(slot - origin) * m_Transform
//
// A constraint that copies a position composes the world transform from its
// target and replaces the translation outright, so the placement has to be
// applied AFTER constraints. Composed before one it is discarded, and the
// mounted artboard renders off by its origin -- visible only when the artboard
// has an origin to lose, which is why the fixture centers it.
//
// The row is rotated as well, so these also pin that the placement arrives in
// the parent's frame rather than left-applied in artboard space.

#include "rive/artboard.hpp"
#include "rive/layout_component.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/nested_artboard_layout.hpp"
#include "rive/node.hpp"
#include "rive/shapes/shape.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <catch.hpp>
#include <cmath>

// From gen_layout_fixtures.py: a 200x200 flex row rotated 90 degrees holding a
// 60x60 fixed sibling and then a NestedArtboardLayout, whose mounted 50x50
// artboard has a 0.5/0.5 origin and a world-space translation constraint
// pointing at a node.
static const char* kAsset = "assets/layout/nested_artboard_constrained.riv";

// The reported case, mirrored: an UNROTATED row, a centered-origin mounted
// artboard, and a real FollowPathConstraint rather than a stand-in. Source and
// dest are left at world with offset off, so the constrained position is the
// path's first vertex -- which sits at the target shape's own origin, and is
// read from the shape rather than baked in.
static const char* kFollowAsset =
    "assets/layout/nested_artboard_followpath.riv";

// A ComponentOrigin on the nested artboard overrides the source artboard's own
// origin (NestedArtboard::applyOriginOverride writes it onto the mounted
// instance). Source is 0.5/0.5, override is 0.25/0.75, so a placement reading
// the source instead would be wrong on both axes.
static const char* kOverrideAsset =
    "assets/layout/nested_artboard_origin_override.riv";

namespace
{
struct Scene
{
    std::unique_ptr<rive::ArtboardInstance> artboard;
    rive::LayoutComponent* row = nullptr;
    rive::NestedArtboardLayout* host = nullptr;
    rive::ArtboardInstance* instance = nullptr;
    rive::Vec2D constrainedTo;
    rive::Vec2D base;
};

// Everything is read back off the loaded file rather than baked in, so these
// pin the composition and not the solver's arithmetic.
Scene load(rive::File* file)
{
    Scene s;
    // An instance, not file->artboard(): nested artboards are only mounted on
    // an ArtboardInstance, so the source artboard has no artboardInstance().
    s.artboard = file->artboardNamed("NestedConstrainedHost");
    REQUIRE(s.artboard != nullptr);
    s.artboard->advance(0.0f);

    for (auto layout : s.artboard->find<rive::LayoutComponent>())
    {
        if (!layout->is<rive::Artboard>() && layout->rotation() != 0.0f)
        {
            s.row = layout;
        }
    }
    REQUIRE(s.row != nullptr);

    auto nested = s.artboard->find<rive::NestedArtboardLayout>();
    REQUIRE(nested.size() == 1);
    s.host = nested[0];
    s.instance = s.host->artboardInstance();
    REQUIRE(s.instance != nullptr);

    auto* target = s.artboard->find<rive::Node>("Target");
    REQUIRE(target != nullptr);
    s.constrainedTo =
        rive::Vec2D(target->worldTransform()[4], target->worldTransform()[5]);

    // The sibling pushes the slot off the row's origin and the centered origin
    // makes the anchor non-zero; without both there is nothing to lose.
    auto slot = rive::Vec2D(s.instance->layoutX(), s.instance->layoutY());
    REQUIRE(slot.x != 0.0f);
    REQUIRE(s.instance->origin().x != 0.0f);
    s.base = slot - s.instance->origin();
    return s;
}
} // namespace

TEST_CASE("a constrained nested artboard keeps its layout placement",
          "[layout]")
{
    auto file = ReadRiveFile(kAsset);
    auto s = load(file.get());

    // The constraint writes where we sit, then the placement lands on top of
    // it, rotated into the row's frame.
    const rive::Mat2D& p = s.row->worldTransform();
    auto expected =
        s.constrainedTo + rive::Vec2D(p[0] * s.base.x + p[2] * s.base.y,
                                      p[1] * s.base.x + p[3] * s.base.y);

    auto world = s.host->worldTransform();
    REQUIRE(world[4] == Approx(expected.x).margin(1e-4));
    REQUIRE(world[5] == Approx(expected.y).margin(1e-4));
}

TEST_CASE("a constraint does not erase the layout placement", "[layout]")
{
    auto file = ReadRiveFile(kAsset);
    auto s = load(file.get());

    // Composing the placement before the constraint leaves exactly the
    // constrained position and nothing else. That is the regression.
    auto world = s.host->worldTransform();
    auto off = std::abs(world[4] - s.constrainedTo.x) +
               std::abs(world[5] - s.constrainedTo.y);
    REQUIRE(off > 1e-4f);
}

TEST_CASE("a re-applied placement still turns with the parent", "[layout]")
{
    auto file = ReadRiveFile(kAsset);
    auto s = load(file.get());

    // Applying it after the constraint must not mean applying it in artboard
    // space again: the placement belongs to the parent, so the rotated row
    // turns it.
    auto legacy = s.constrainedTo + s.base;

    auto world = s.host->worldTransform();
    REQUIRE(world[4] != Approx(legacy.x).margin(1e-4));
}

namespace
{
struct FollowScene
{
    std::unique_ptr<rive::ArtboardInstance> artboard;
    rive::NestedArtboardLayout* host = nullptr;
    rive::Vec2D landed;
    rive::Vec2D base;
};

FollowScene loadFollow(rive::File* file)
{
    FollowScene s;
    s.artboard = file->artboardNamed("NestedFollowPathHost");
    REQUIRE(s.artboard != nullptr);
    s.artboard->advance(0.0f);

    auto nested = s.artboard->find<rive::NestedArtboardLayout>();
    REQUIRE(nested.size() == 1);
    s.host = nested[0];
    auto* instance = s.host->artboardInstance();
    REQUIRE(instance != nullptr);

    auto* target = s.artboard->find<rive::Shape>("FollowTarget");
    REQUIRE(target != nullptr);
    s.landed =
        rive::Vec2D(target->worldTransform()[4], target->worldTransform()[5]);

    // A centered origin is what the constraint used to erase; at 0/0 there is
    // nothing to lose.
    REQUIRE(instance->origin().x != 0.0f);
    REQUIRE(instance->origin().y != 0.0f);
    s.base = rive::Vec2D(instance->layoutX(), instance->layoutY()) -
             instance->origin();
    return s;
}
} // namespace

TEST_CASE("a follow path constraint keeps the layout placement", "[layout]")
{
    auto file = ReadRiveFile(kFollowAsset);
    auto s = loadFollow(file.get());

    // Nothing above the row rotates or scales, so the placement lands on the
    // constrained position unturned.
    auto world = s.host->worldTransform();
    REQUIRE(world[4] == Approx(s.landed.x + s.base.x).margin(1e-4));
    REQUIRE(world[5] == Approx(s.landed.y + s.base.y).margin(1e-4));
}

TEST_CASE("a follow path constraint does not erase the placement", "[layout]")
{
    auto file = ReadRiveFile(kFollowAsset);
    auto s = loadFollow(file.get());

    // Composed before the constraint, the placement is gone and the nested
    // artboard sits exactly on the path. That is the reported regression.
    auto world = s.host->worldTransform();
    auto off =
        std::abs(world[4] - s.landed.x) + std::abs(world[5] - s.landed.y);
    REQUIRE(off > 1e-4f);
}

TEST_CASE("the placement uses an overridden nested artboard origin", "[layout]")
{
    auto file = ReadRiveFile(kOverrideAsset);
    auto artboard = file->artboardNamed("OriginOverrideHost");
    REQUIRE(artboard != nullptr);
    artboard->advance(0.0f);

    auto nested = artboard->find<rive::NestedArtboardLayout>();
    REQUIRE(nested.size() == 1);
    auto* host = nested[0];
    auto* instance = host->artboardInstance();
    REQUIRE(instance != nullptr);

    auto* target = artboard->find<rive::Node>("Target");
    REQUIRE(target != nullptr);
    rive::Vec2D landed(target->worldTransform()[4],
                       target->worldTransform()[5]);

    // The override reached the instance: 0.25/0.75, not the source's 0.5/0.5.
    auto origin = instance->origin();
    REQUIRE(origin.x == Approx(-0.25f * instance->layoutWidth()).margin(1e-4));
    REQUIRE(origin.y == Approx(-0.75f * instance->layoutHeight()).margin(1e-4));
    REQUIRE(instance->layoutWidth() > 0.0f);
    REQUIRE(instance->layoutHeight() > 0.0f);

    // And the placement re-applied after the constraint uses it.
    auto base = rive::Vec2D(instance->layoutX(), instance->layoutY()) - origin;
    auto world = host->worldTransform();
    REQUIRE(world[4] == Approx(landed.x + base.x).margin(1e-4));
    REQUIRE(world[5] == Approx(landed.y + base.y).margin(1e-4));

    // Reading the source origin instead would land it somewhere else entirely.
    auto fromSource = rive::Vec2D(instance->layoutX(), instance->layoutY()) -
                      rive::Vec2D(-0.5f * instance->layoutWidth(),
                                  -0.5f * instance->layoutHeight());
    REQUIRE(world[5] != Approx(landed.y + fromSource.y).margin(1e-4));
}

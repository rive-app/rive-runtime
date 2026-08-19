/*
 * Copyright 2026 Rive
 */

// A NestedArtboardLayout is placed by the layout engine, so its world transform
// composes the solved slot INSIDE the parent's frame:
//
//   parentWorld * translate(slot) * ours * translate(-origin)
//
// It used to left-apply the slot in artboard space. That is indistinguishable
// while nothing above it can rotate — translations commute — and wrong the
// moment the parent turns, which layouts becoming transformable made reachable.
//
// The assertions are written against the parent's actual world transform and
// the actual solved slot rather than baked-in numbers, so they pin the
// composition rather than the solver's arithmetic.

#include "rive/artboard.hpp"
#include "rive/layout_component.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/nested_artboard_layout.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <catch.hpp>

// From gen_layout_fixtures.py: a 200x200 flex row rotated 90 degrees, holding a
// 60x60 fixed sibling and then a NestedArtboardLayout, so the nested artboard's
// slot is (60, 0) rather than the origin. The sibling is what makes this
// observable — at slot (0, 0) both compositions agree at any rotation.
static const char* kAsset = "assets/layout/nested_artboard_rotated.riv";

static rive::LayoutComponent* rotatedRow(rive::Artboard* artboard)
{
    for (auto layout : artboard->find<rive::LayoutComponent>())
    {
        if (!layout->is<rive::Artboard>() && layout->rotation() != 0.0f)
        {
            return layout;
        }
    }
    return nullptr;
}

TEST_CASE("a nested artboard's slot composes inside a rotated parent",
          "[layout]")
{
    auto file = ReadRiveFile(kAsset);
    // An instance, not file->artboard(): nested artboards are only mounted on
    // an ArtboardInstance, so the source artboard has no artboardInstance().
    auto artboard = file->artboardNamed("NestedRotatedHost");
    REQUIRE(artboard != nullptr);
    artboard->advance(0.0f);

    auto* row = rotatedRow(artboard.get());
    REQUIRE(row != nullptr);

    auto nested = artboard->find<rive::NestedArtboardLayout>();
    REQUIRE(nested.size() == 1);
    auto* host = nested[0];

    auto* instance = host->artboardInstance();
    REQUIRE(instance != nullptr);

    // The sibling pushes it off the row's origin; without that this test cannot
    // tell the two compositions apart.
    auto slot = rive::Vec2D(instance->layoutX(), instance->layoutY());
    REQUIRE(slot.x != 0.0f);

    auto expected = row->worldTransform() * rive::Mat2D::fromTranslation(slot) *
                    host->transform() *
                    rive::Mat2D::fromTranslation(-instance->origin());

    auto world = host->worldTransform();
    REQUIRE(world[4] == Approx(expected[4]).margin(1e-4));
    REQUIRE(world[5] == Approx(expected[5]).margin(1e-4));
}

TEST_CASE("a rotated parent does not leave the slot axis-aligned", "[layout]")
{
    auto file = ReadRiveFile(kAsset);
    // An instance, not file->artboard(): nested artboards are only mounted on
    // an ArtboardInstance, so the source artboard has no artboardInstance().
    auto artboard = file->artboardNamed("NestedRotatedHost");
    REQUIRE(artboard != nullptr);
    artboard->advance(0.0f);

    auto* row = rotatedRow(artboard.get());
    REQUIRE(row != nullptr);
    auto nested = artboard->find<rive::NestedArtboardLayout>();
    REQUIRE(nested.size() == 1);
    auto* host = nested[0];
    auto* instance = host->artboardInstance();
    REQUIRE(instance != nullptr);

    // The old composition, left-applying the slot in artboard space. With the
    // row rotated and the slot non-zero this lands somewhere else entirely, so
    // the regression reappearing fails here rather than silently passing above.
    auto slot = rive::Vec2D(instance->layoutX(), instance->layoutY());
    auto legacy = rive::Mat2D::fromTranslation(slot - instance->origin()) *
                  row->worldTransform() * host->transform();

    auto world = host->worldTransform();
    REQUIRE(world[4] != Approx(legacy[4]).margin(1e-4));
}

TEST_CASE("a rotated parent's basis reaches the nested artboard", "[layout]")
{
    auto file = ReadRiveFile(kAsset);
    auto artboard = file->artboardNamed("NestedRotatedHost");
    REQUIRE(artboard != nullptr);
    artboard->advance(0.0f);

    auto* row = rotatedRow(artboard.get());
    REQUIRE(row != nullptr);
    auto nested = artboard->find<rive::NestedArtboardLayout>();
    REQUIRE(nested.size() == 1);
    auto* host = nested[0];

    // Translation is only half of it. The nested artboard has no transform of
    // its own and the anchor is a pure translation, so its basis must be the
    // row's basis exactly -- a fix that landed the position but mangled the
    // rotation would still pass the tests above.
    auto world = host->worldTransform();
    auto parent = row->worldTransform();
    REQUIRE(world[0] == Approx(parent[0]).margin(1e-4));
    REQUIRE(world[1] == Approx(parent[1]).margin(1e-4));
    REQUIRE(world[2] == Approx(parent[2]).margin(1e-4));
    REQUIRE(world[3] == Approx(parent[3]).margin(1e-4));
}

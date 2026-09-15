/*
 * Copyright 2026 Rive
 */

// A layout's box starts at its local zero, so constraints step back by its
// origin — landing that origin on the target rather than the box corner, or
// holding it fixed while the linear part changes. Mirrors
// rive_core/test/layout_constraint_anchor_test.dart.

#include <rive/artboard.hpp>
#include <rive/component_origin.hpp>
#include <rive/constraints/rotation_constraint.hpp>
#include <rive/constraints/scale_constraint.hpp>
#include <rive/constraints/translation_constraint.hpp>
#include <rive/layout/layout_component_style.hpp>
#include <rive/layout_component.hpp>
#include "rive/math/math_types.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <catch.hpp>

using namespace rive;

// assets/layout/stack.riv puts a fixed 40x40 box at (160,160).
static LayoutComponent* fixedBox(Artboard* artboard)
{
    LayoutComponent* box = nullptr;
    for (auto layout : artboard->find<LayoutComponent>())
    {
        if (layout->is<Artboard>() || layout->style() == nullptr ||
            layout->style()->isStack() ||
            layout->style()->widthScaleType() == LayoutScaleType::fill)
        {
            continue;
        }
        box = layout;
    }
    return box;
}

// Parent through onAddedDirty rather than addChild alone: addChild only fills
// the child list, and both objects need parent() resolved to register.
static ComponentOrigin* addOrigin(Artboard* artboard,
                                  LayoutComponent* owner,
                                  float x,
                                  float y)
{
    auto origin = new ComponentOrigin();
    artboard->addObject(origin);
    origin->parentId(artboard->idOf(owner));
    REQUIRE(origin->onAddedDirty(artboard) == StatusCode::Ok);
    origin->originX(x);
    origin->originY(y);
    return origin;
}

template <typename T>
static T* addConstraint(Artboard* artboard,
                        LayoutComponent* owner,
                        Core* target)
{
    auto constraint = new T();
    artboard->addObject(constraint);
    constraint->parentId(artboard->idOf(owner));
    constraint->targetId(artboard->idOf(target));
    REQUIRE(constraint->onAddedDirty(artboard) == StatusCode::Ok);
    constraint->markConstraintDirty();
    return constraint;
}

// Where the box's anchor sits in world space.
static Vec2D anchorOf(LayoutComponent* box)
{
    return box->worldTransform() * box->localAnchor();
}

TEST_CASE("a translation constraint lands a layout's origin on the target",
          "[layout]")
{
    auto file = ReadRiveFile("assets/layout/stack.riv");
    auto artboard = file->artboard();
    REQUIRE(artboard != nullptr);
    artboard->advance(0.0f);

    auto* box = fixedBox(artboard);
    REQUIRE(box != nullptr);
    REQUIRE(box->layoutWidth() == 40.0f);

    // Centre origin, so the anchor is 20 into the 40x40 box.
    addOrigin(artboard, box, 0.5f, 0.5f);
    addConstraint<TranslationConstraint>(artboard, box, artboard);
    artboard->advance(0.0f);

    REQUIRE(box->localAnchor().x == Approx(20.0f));

    // The origin lands on the target, so the box corner sits back by the
    // anchor — without the correction the corner would be on the target.
    auto target = artboard->worldTransform();
    REQUIRE(anchorOf(box).x == Approx(target[4]));
    REQUIRE(anchorOf(box).y == Approx(target[5]));
    REQUIRE(box->worldTransform()[4] == Approx(target[4] - 20.0f));
    REQUIRE(box->worldTransform()[5] == Approx(target[5] - 20.0f));
}

TEST_CASE("a layout with no origin is left uncorrected", "[layout]")
{
    auto file = ReadRiveFile("assets/layout/stack.riv");
    auto artboard = file->artboard();
    REQUIRE(artboard != nullptr);
    artboard->advance(0.0f);

    auto* box = fixedBox(artboard);
    REQUIRE(box != nullptr);

    addConstraint<TranslationConstraint>(artboard, box, artboard);
    artboard->advance(0.0f);

    // No anchor, so the box corner lands on the target exactly as it always
    // did. This is what keeps existing files unchanged.
    REQUIRE(box->localAnchor().x == 0.0f);
    auto target = artboard->worldTransform();
    REQUIRE(box->worldTransform()[4] == Approx(target[4]));
    REQUIRE(box->worldTransform()[5] == Approx(target[5]));
}

TEST_CASE("a rotation constraint spins a layout about its origin", "[layout]")
{
    auto file = ReadRiveFile("assets/layout/stack.riv");
    auto artboard = file->artboard();
    REQUIRE(artboard != nullptr);

    auto* box = fixedBox(artboard);
    REQUIRE(box != nullptr);
    addOrigin(artboard, box, 0.5f, 0.5f);
    // Rotate the box so constraining it to the unrotated artboard changes the
    // linear part; without that the test would pass on a no-op.
    box->rotation(math::PI / 2.0f);
    artboard->advance(0.0f);

    auto before = anchorOf(box);
    auto linearBefore = box->worldTransform()[0];

    addConstraint<RotationConstraint>(artboard, box, artboard);
    artboard->advance(0.0f);

    // Rotation keeps the translation and swaps the linear part, so the anchor
    // must be put back — otherwise the box swings about its corner.
    REQUIRE(anchorOf(box).x == Approx(before.x).margin(0.001f));
    REQUIRE(anchorOf(box).y == Approx(before.y).margin(0.001f));
    REQUIRE(std::abs(box->worldTransform()[0] - linearBefore) > 0.01f);
}

TEST_CASE("a scale constraint scales a layout about its origin", "[layout]")
{
    auto file = ReadRiveFile("assets/layout/stack.riv");
    auto artboard = file->artboard();
    REQUIRE(artboard != nullptr);

    auto* box = fixedBox(artboard);
    REQUIRE(box != nullptr);
    addOrigin(artboard, box, 0.5f, 0.5f);
    box->scaleX(2.0f);
    box->scaleY(2.0f);
    artboard->advance(0.0f);

    auto before = anchorOf(box);
    auto linearBefore = box->worldTransform()[0];

    addConstraint<ScaleConstraint>(artboard, box, artboard);
    artboard->advance(0.0f);

    REQUIRE(anchorOf(box).x == Approx(before.x).margin(0.001f));
    REQUIRE(anchorOf(box).y == Approx(before.y).margin(0.001f));
    REQUIRE(std::abs(box->worldTransform()[0] - linearBefore) > 0.01f);
}

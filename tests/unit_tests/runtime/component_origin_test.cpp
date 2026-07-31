/*
 * Copyright 2026 Rive
 */

// Verifies that a ComponentOrigin child gives its owner an origin: on a nested
// artboard it overrides the mounted instance's origin, on a layout it is the
// pivot rotation/scale compose about. Owners without the child are left
// untouched (the zero-cost common case).

#include <rive/artboard.hpp>
#include <rive/nested_artboard.hpp>
#include <rive/component_origin.hpp>
#include <rive/layout/layout_component_style.hpp>
#include <rive/layout_component.hpp>
#include "rive/math/math_types.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include "utils/serializing_factory.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include <catch.hpp>

using namespace rive;

TEST_CASE("ComponentOrigin overrides the mounted instance origin", "[file]")
{
    auto file = ReadRiveFile("assets/nested_artboard_opacity.riv");

    auto mainArtboard = file->artboard()->instance();
    auto artboard = mainArtboard->find<Artboard>("Parent Artboard");
    REQUIRE(artboard != nullptr);
    artboard->updateComponents();

    auto nested = artboard->find<NestedArtboard>("Nested artboard container");
    REQUIRE(nested != nullptr);
    auto instance = nested->artboardInstance();
    REQUIRE(instance != nullptr);

    // Control: with no override child, applyOriginOverride() is a no-op.
    instance->originX(0.0f);
    instance->originY(0.0f);
    nested->applyOriginOverride();
    REQUIRE(instance->originX() == 0.0f);
    REQUIRE(instance->originY() == 0.0f);

    // Author an origin override as a child of the nested artboard. Ownership is
    // handed to the artboard's object list so it is freed at teardown (mirrors
    // how imported objects are owned).
    auto origin = new ComponentOrigin();
    origin->originX(0.25f);
    origin->originY(0.75f);
    nested->addChild(origin);
    artboard->addObject(origin);

    nested->applyOriginOverride();
    REQUIRE(instance->originX() == 0.25f);
    REQUIRE(instance->originY() == 0.75f);
}

// The same child on a LayoutComponent is the pivot its rotation/scale compose
// about. assets/layout/stack.riv puts a fixed 40x40 box at (160,160).
TEST_CASE("ComponentOrigin pivots a layout's transform", "[layout]")
{
    auto file = ReadRiveFile("assets/layout/stack.riv");
    auto artboard = file->artboard();
    REQUIRE(artboard != nullptr);
    artboard->advance(0.0f);

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
    REQUIRE(box != nullptr);
    REQUIRE(box->layoutWidth() == 40.0f);

    // Control: rotating with no origin child pivots at the slot's top-left, so
    // the translation is the slot itself. Check the rotation landed too —
    // translation alone can't tell an applied rotation from a skipped update.
    box->rotation(math::PI / 2.0f);
    artboard->advance(0.0f);
    REQUIRE(box->worldTransform().xx() == Approx(0.0f).margin(1e-6));
    REQUIRE(box->worldTransform().xy() == Approx(1.0f));
    REQUIRE(box->worldTransform().tx() == Approx(160.0f));
    REQUIRE(box->worldTransform().ty() == Approx(160.0f));

    // Pivot at the box's center: the 90 degree rotation now swings the box
    // about (20,20), moving its origin to (200,160). Parent through
    // onAddedDirty rather than addChild alone — addChild only fills the child
    // list, and the origin needs parent() to dirty its owner on a change.
    auto origin = new ComponentOrigin();
    artboard->addObject(origin);
    origin->parentId(artboard->idOf(box));
    REQUIRE(origin->onAddedDirty(artboard) == StatusCode::Ok);
    REQUIRE(origin->parent() == box);
    origin->originX(0.5f);
    origin->originY(0.5f);
    artboard->advance(0.0f);
    REQUIRE(box->worldTransform().tx() == Approx(200.0f));
    REQUIRE(box->worldTransform().ty() == Approx(160.0f));
}

TEST_CASE("Animated origin and click events", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/nested_artboard_origin_override_test.riv",
                             &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    auto renderer = silver.makeRenderer();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->pointerDown(Vec2D(250, 250));
        stateMachine->pointerUp(Vec2D(250, 250));
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("nested_artboard_origin_override_test"));
}
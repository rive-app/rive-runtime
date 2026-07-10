/*
 * Copyright 2026 Rive
 */

// Verifies that a NestedArtboardOrigin child overrides the origin of the
// mounted artboard instance, and that a nested artboard without the child is
// left untouched (the zero-cost common case).

#include <rive/artboard.hpp>
#include <rive/nested_artboard.hpp>
#include <rive/nested_artboard_origin.hpp>
#include "rive_file_reader.hpp"
#include "utils/serializing_factory.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include <catch.hpp>

using namespace rive;

TEST_CASE("NestedArtboardOrigin overrides the mounted instance origin",
          "[file]")
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
    auto origin = new NestedArtboardOrigin();
    origin->originX(0.25f);
    origin->originY(0.75f);
    nested->addChild(origin);
    artboard->addObject(origin);

    nested->applyOriginOverride();
    REQUIRE(instance->originX() == 0.25f);
    REQUIRE(instance->originY() == 0.75f);
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
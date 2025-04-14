#include "rive/file.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "utils/serializing_factory.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>
#include <cstring>

using namespace rive;

TEST_CASE("juice silver", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/juice.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto walkAnimation = artboard->animationNamed("walk");
    REQUIRE(walkAnimation != nullptr);

    auto renderer = silver.makeRenderer();
    // Draw first frame.
    walkAnimation->advanceAndApply(0.0f);
    artboard->draw(renderer.get());

    int frames = (int)(walkAnimation->durationSeconds() / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        walkAnimation->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("juice"));
}

TEST_CASE("n-slice silver", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/n_slice_triangle.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());
    artboard->advance(0.0f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    CHECK(silver.matches("n_slice_triangle"));
}

TEST_CASE("lock icon listener silver", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/lock_icon_demo.riv", &silver);

    auto artboard = file->artboardDefault();
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    silver.addFrame();

    // Click in the middle of the state machine.
    stateMachine->pointerDown(
        rive::Vec2D(artboard->width() / 2.0f, artboard->height() / 2.0f));
    // Advance and apply twice to take the transition and apply the next state.
    stateMachine->advanceAndApply(0.1f);
    stateMachine->advanceAndApply(1.0f);

    artboard->draw(renderer.get());

    silver.addFrame();

    // Do it again to lock the icon.
    stateMachine->pointerDown(
        rive::Vec2D(artboard->width() / 2.0f, artboard->height() / 2.0f));
    // Advance and apply twice to take the transition and apply the next state.
    stateMachine->advanceAndApply(0.1f);
    stateMachine->advanceAndApply(1.0f);

    artboard->draw(renderer.get());

    CHECK(silver.matches("lock_icon_demo"));
}

TEST_CASE("validate text run listener works", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/text_listener_simpler.riv", &silver);

    auto artboard = file->artboardDefault();
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    silver.addFrame();

    // Click in the middle of the state machine.
    stateMachine->pointerDown(
        rive::Vec2D(artboard->width() * 0.8, artboard->height() / 2.0f));
    // Advance and apply twice to take the transition and apply the next state.
    stateMachine->advanceAndApply(0.1f);
    stateMachine->advanceAndApply(1.0f);

    artboard->draw(renderer.get());

    CHECK(silver.matches("text_listener_simpler"));
}

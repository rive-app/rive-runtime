#include "rive/file.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"
#include "utils/serializing_factory.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>
#include <cstring>

using namespace rive;

TEST_CASE("multiple scrolling artboards", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/scroll_test.riv", &silver);

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

    // Start scroll on right element
    stateMachine->pointerDown(rive::Vec2D(260.0f, 500.0f));
    // Advance and apply twice to take the transition and apply the next state.
    stateMachine->advanceAndApply(0.1f);
    stateMachine->advanceAndApply(1.0f);
    artboard->draw(renderer.get());

    float yMovement = 400.0f;
    float xMovement = 100.0f;

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->pointerMove(
            rive::Vec2D(260.0f - (i * xMovement / frames),
                        500.0f - (i * yMovement / frames)));
        // Advance and apply twice to take the transition and apply the next
        // state.
        stateMachine->advanceAndApply(0.1f);
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }
    silver.addFrame();
    stateMachine->pointerUp(
        rive::Vec2D(260.0f - xMovement, 500.0f - yMovement));
    stateMachine->advanceAndApply(0.1f);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    silver.addFrame();

    // Start scroll on left element
    stateMachine->pointerDown(rive::Vec2D(50.0f, 500.0f));
    // Advance and apply twice to take the transition and apply the next state.
    stateMachine->advanceAndApply(0.1f);
    stateMachine->advanceAndApply(1.0f);
    artboard->draw(renderer.get());

    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->pointerMove(
            rive::Vec2D(50.0f + (i * xMovement / frames),
                        500.0f - (i * yMovement / frames)));
        // Advance and apply twice to take the transition and apply the next
        // state.
        stateMachine->advanceAndApply(0.1f);
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }
    silver.addFrame();
    stateMachine->pointerUp(rive::Vec2D(50.0f + xMovement, 500.0f - yMovement));
    stateMachine->advanceAndApply(0.1f);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("scroll_test"));
}

TEST_CASE("Vertical scroll with threshold", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/scroll_threshold.riv", &silver);

    auto artboard = file->artboardNamed("vertical-scroll");
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    auto vmi = file->createViewModelInstance(artboard.get()->viewModelId(), 0);
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    silver.addFrame();

    // Threshold value is 40

    float pos = 70.0f;
    stateMachine->pointerDown(rive::Vec2D(artboard->width() / 2.0f, pos));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    while (pos > 40)
    {

        silver.addFrame();
        stateMachine->pointerMove(rive::Vec2D(artboard->width() / 2.0f, pos));
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
        pos -= 8;
    }
    // Since it didn't go over the threshold, element shouldn't have scrolled
    // and nested item should trigger its click action
    silver.addFrame();
    stateMachine->pointerUp(rive::Vec2D(artboard->width() / 2.0f, pos));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    // Scroll beyond threshold
    pos = 70.0f;
    stateMachine->pointerDown(rive::Vec2D(artboard->width() / 2.0f, pos));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    while (pos > 10)
    {

        silver.addFrame();
        stateMachine->pointerMove(rive::Vec2D(artboard->width() / 2.0f, pos));
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
        pos -= 8;
    }
    // Scrolled beyond threshold and click action is not fired
    silver.addFrame();
    stateMachine->pointerUp(rive::Vec2D(artboard->width() / 2.0f, pos));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("scroll_threshold-vertical-scroll"));
}

TEST_CASE("Horizontal scroll with threshold", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/scroll_threshold.riv", &silver);

    auto artboard = file->artboardNamed("horizontal-scroll");
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    auto vmi = file->createViewModelInstance(artboard.get()->viewModelId(), 0);
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    silver.addFrame();

    // Threshold value is 40

    float pos = 70.0f;
    stateMachine->pointerDown(rive::Vec2D(pos, artboard->height() / 2.0f));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    while (pos > 40)
    {

        silver.addFrame();
        stateMachine->pointerMove(rive::Vec2D(pos, artboard->height() / 2.0f));
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
        pos -= 8;
    }
    // Since it didn't go over the threshold, element shouldn't have scrolled
    // and nested item should trigger its click action
    silver.addFrame();
    stateMachine->pointerUp(rive::Vec2D(pos, artboard->height() / 2.0f));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    // Scroll beyond threshold
    pos = 70.0f;
    stateMachine->pointerDown(rive::Vec2D(pos, artboard->height() / 2.0f));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    while (pos > 10)
    {

        silver.addFrame();
        stateMachine->pointerMove(rive::Vec2D(pos, artboard->height() / 2.0f));
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
        pos -= 8;
    }
    // Scrolled beyond threshold and click action is not fired
    silver.addFrame();
    stateMachine->pointerUp(rive::Vec2D(pos, artboard->height() / 2.0f));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("scroll_threshold-horizontal-scroll"));
}

TEST_CASE("Multidirectional scroll with threshold", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/scroll_threshold.riv", &silver);

    auto artboard = file->artboardNamed("all-scroll");
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    auto vmi = file->createViewModelInstance(artboard.get()->viewModelId(), 0);
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    silver.addFrame();

    // Threshold value is 40

    float pos = 70.0f;
    stateMachine->pointerDown(rive::Vec2D(pos, pos));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    while (pos > 50)
    {

        silver.addFrame();
        stateMachine->pointerMove(rive::Vec2D(pos, pos));
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
        pos -= 8;
    }
    // Since it didn't go over the threshold, element shouldn't have scrolled
    // and nested item should trigger its click action
    silver.addFrame();
    stateMachine->pointerUp(rive::Vec2D(pos, pos));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    // Scroll beyond threshold
    pos = 70.0f;
    stateMachine->pointerDown(rive::Vec2D(pos, pos));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    while (pos > 32)
    {

        silver.addFrame();
        stateMachine->pointerMove(rive::Vec2D(pos, pos));
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
        pos -= 8;
    }
    // Scrolled diagonnally beyond threshold and click action is not fired
    silver.addFrame();
    stateMachine->pointerUp(rive::Vec2D(pos, pos));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("scroll_threshold-all-scroll"));
}

#include <rive/solo.hpp>
#include <rive/nested_artboard.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/animation/nested_linear_animation.hpp>
#include <rive/animation/nested_state_machine.hpp>
#include <rive/viewmodel/viewmodel_instance_number.hpp>
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include "utils/serializing_factory.hpp"
#include <catch.hpp>
#include <cstdio>
#include <iostream>

TEST_CASE("collapsed nested artboards do not advance", "[solo]")
{
    auto file = ReadRiveFile("assets/solos_with_nested_artboards.riv");

    auto artboard = file->artboard("main-artboard")->instance();
    artboard->advance(0.0f);
    auto solos = artboard->find<rive::Solo>();
    REQUIRE(solos.size() == 1);
    auto stateMachine = artboard->stateMachineAt(0);
    stateMachine->advanceAndApply(0.0f);
    artboard->advance(0.75f);
    // Testing whether the squares have moved in the artboard is an indirect way
    // if checking whether the time of each artboard has advanced
    // Unfortunately there is no way of accessing the time of the animations
    // directly
    auto redNestedArtboard =
        stateMachine->artboard()->find<rive::NestedArtboard>("red-artboard");
    auto redNestedArtboardArtboard = redNestedArtboard->artboardInstance();
    auto movingShapes = redNestedArtboardArtboard->find<rive::Shape>();
    auto redRect = movingShapes.at(0);
    REQUIRE(redRect->x() > 50);
    auto greenNestedArtboard =
        stateMachine->artboard()->find<rive::NestedArtboard>("green-artboard");
    auto greenNestedArtboardArtboard = greenNestedArtboard->artboardInstance();
    auto greenMovingShapes = greenNestedArtboardArtboard->find<rive::Shape>();
    auto greenRect = greenMovingShapes.at(0);
    REQUIRE(greenRect->x() == 50);
}

TEST_CASE("nested artboards with looping animations will keep main "
          "advanceAndApply advancing",
          "[nested]")
{
    auto file = ReadRiveFile("assets/ball_test.riv");
    auto artboard = file->artboard("Artboard")->instance();
    artboard->advance(0.0f);
    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine->advanceAndApply(0.0f) == true);
    REQUIRE(stateMachine->advanceAndApply(1.0f) == true);
    REQUIRE(stateMachine->advanceAndApply(1.0f) == true);
}

TEST_CASE("nested artboards with one shot animations will not main "
          "advanceAndApply advancing",
          "[nested]")
{
    auto file = ReadRiveFile("assets/ball_test.riv");
    auto artboard = file->artboard("Artboard 2")->instance();
    artboard->advance(0.0f);
    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine->advanceAndApply(0.0f) == true);
    REQUIRE(stateMachine->advanceAndApply(0.9f) == true);
    REQUIRE(stateMachine->advanceAndApply(0.1f) == true);
    // nested artboards animation is 1s long but has an event to complete
    REQUIRE(stateMachine->advanceAndApply(0.1f) == true);
    // animation has ended and no events to report
    REQUIRE(stateMachine->advanceAndApply(0.1f) == false);
}

TEST_CASE(
    "nested artboard remapped timelines controlled by a timeline controlled "
    "itself by a joystick correctly apply the time after the joystick ran",
    "[nested]")
{
    auto file = ReadRiveFile("assets/joystick_nested_remap.riv");
    auto artboard = file->artboard("parent")->instance();
    artboard->advance(0.0f);
    auto stateMachine = artboard->stateMachineAt(0);

    REQUIRE(artboard->find<rive::NestedArtboard>("child") != nullptr);

    auto child = artboard->find<rive::NestedArtboard>("child");

    auto nestedArtboardArtboard = child->artboardInstance();
    REQUIRE(nestedArtboardArtboard != nullptr);
    auto rect = nestedArtboardArtboard->find<rive::Shape>("rect");
    REQUIRE(rect != nullptr);
    REQUIRE(rect->x() == 250.0f);
}

TEST_CASE("Nested events don't conflict with parent events.", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/nested_events.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());
    // Pointer down on left top square, triggers child event 1
    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(50.0f, 50.0f));
    // Advance and apply twice to take the transition and apply the next state.
    stateMachine->advanceAndApply(0.1f);
    stateMachine->advanceAndApply(1.0f);
    artboard->draw(renderer.get());

    // Pointer down on right top square, triggers child event 2
    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(150.0f, 50.0f));
    // Advance and apply twice to take the transition and apply the next state.
    stateMachine->advanceAndApply(0.1f);
    stateMachine->advanceAndApply(1.0f);
    artboard->draw(renderer.get());

    // Pointer down on left bottom square, triggers parent event 1
    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(50.0f, 150.0f));
    // Advance and apply twice to take the transition and apply the next state.
    stateMachine->advanceAndApply(0.1f);
    stateMachine->advanceAndApply(1.0f);
    artboard->draw(renderer.get());

    // Pointer down on right bottom square, triggers parent event 1
    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(150.0f, 150.0f));
    // Advance and apply twice to take the transition and apply the next state.
    stateMachine->advanceAndApply(0.1f);
    stateMachine->advanceAndApply(1.0f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("nested_events"));
}

TEST_CASE("All nested artboard modes respect hug", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/nested_hug.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());
    silver.addFrame();

    CHECK(silver.matches("nested_hug"));
}

TEST_CASE("Pause and resume nested artboards", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/pause_nested_artboard.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    auto vmi = file->createViewModelInstance(artboard.get()->viewModelId(), 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = (int)(0.25f / 0.016f);
    // State machine advances normally
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    // Click toggles rectangle color
    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(250.0f, 250.0f));
    stateMachine->pointerUp(rive::Vec2D(250.0f, 250.0f));
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    // Toggle data bind pauses state machine
    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(25.0f, 25.0f));
    stateMachine->pointerUp(rive::Vec2D(25.0f, 25.0f));
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    // State machine does not advance
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    // Click does not toggle color back
    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(250.0f, 250.0f));
    stateMachine->pointerUp(rive::Vec2D(250.0f, 250.0f));
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    // Toggle data bind resumes state machine
    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(25.0f, 25.0f));
    stateMachine->pointerUp(rive::Vec2D(25.0f, 25.0f));
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    // Click toggles color back
    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(250.0f, 250.0f));
    stateMachine->pointerUp(rive::Vec2D(250.0f, 250.0f));
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    // State machine advances again
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("pause_nested_artboard"));
}

TEST_CASE("Quantize and speed on nested artboards", "[silver]")
{
    rive::SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/nested_artboard_quantize_and_speed.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    auto vmi = file->createViewModelInstance(artboard.get()->viewModelId(), 0);

    auto speedProp =
        vmi->propertyValue("speed")->as<rive::ViewModelInstanceNumber>();
    auto quantizeProp =
        vmi->propertyValue("quant")->as<rive::ViewModelInstanceNumber>();

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.016f);
    // Multiple state machines in different speed and quantize modes
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    // Update speed and quantize values
    speedProp->propertyValue(4);
    quantizeProp->propertyValue(7);
    stateMachine->advanceAndApply(0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("nested_artboard_quantize_and_speed"));
}
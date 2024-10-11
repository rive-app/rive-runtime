/*
 * Copyright 2022 Rive
 */

#include <rive/math/aabb.hpp>
#include <rive/math/hit_test.hpp>
#include <rive/nested_artboard.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/animation/state_machine_input_instance.hpp>
#include <rive/animation/nested_state_machine.hpp>
#include <rive/animation/animation_state.hpp>
#include "rive_file_reader.hpp"

#include <catch.hpp>
#include <cstdio>

using namespace rive;

TEST_CASE("hittest-basics", "[hittest]")
{
    HitTester tester;
    tester.reset({10, 10, 12, 12});
    tester.move({0, 0});
    tester.line({20, 0});
    tester.line({20, 20});
    tester.line({0, 20});
    tester.close();
    REQUIRE(tester.test());

    IAABB area = {81, 156, 84, 159};

    Vec2D pts[] = {
        {29.9785f, 32.5261f},
        {231.102f, 32.5261f},
        {231.102f, 269.898f},
        {29.9785f, 269.898f},
    };
    tester.reset(area);

    tester.move(pts[0]);
    for (int i = 1; i < 4; ++i)
    {
        tester.line(pts[i]);
    }
    tester.close();
    REQUIRE(tester.test());
}

TEST_CASE("hittest-mesh", "[hittest]")
{

    const IAABB area{10, 10, 12, 12};

    Vec2D verts[] = {
        {0, 0},
        {20, 10},
        {0, 20},
    };
    uint16_t indices[] = {
        0,
        1,
        2,
    };
    REQUIRE(
        HitTester::testMesh(area, make_span(verts, 3), make_span(indices, 3)));
}

TEST_CASE("hit test on opaque target", "[hittest]")
{
    // This artboard has two rects of size 200 x 200, "red-activate" at [0, 0,
    // 200, 200] and "green-activate" at [0, 100, 200, 300] "red-activate" is
    // above "green-activate" in drawing order Both targets are set as opaque
    // for its listeners "red-activate" sets "toGreen" to false "green-activate"
    // sets "toGreen" to true There is also a "gray-activate" above the other 2
    // that is not opaque so events should traverse through the other targets
    auto file = ReadRiveFile("assets/opaque_hit_test.riv");

    auto artboard = file->artboard("main");
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("main-state-machine");

    REQUIRE(artboardInstance != nullptr);
    REQUIRE(artboardInstance->stateMachineCount() == 1);

    REQUIRE(stateMachine != nullptr);

    rive::StateMachineInstance* stateMachineInstance =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());

    stateMachineInstance->advance(0.0f);
    artboardInstance->advance(0.0f);
    REQUIRE(stateMachineInstance->needsAdvance() == true);
    stateMachineInstance->advance(0.0f);

    auto toGreenToggle = stateMachineInstance->getBool("toGreen");
    REQUIRE(toGreenToggle != nullptr);
    auto grayToggle = stateMachineInstance->getBool("grayToggle");
    REQUIRE(grayToggle != nullptr);

    stateMachineInstance->pointerDown(rive::Vec2D(100.0f, 50.0f));
    // "gray-activate" is clicked
    REQUIRE(grayToggle->value() == true);
    // Pointer only over "red-activate"
    REQUIRE(toGreenToggle->value() == false);

    stateMachineInstance->pointerDown(rive::Vec2D(100.0f, 250.0f));
    // "gray-activate" is clicked
    REQUIRE(grayToggle->value() == false);
    // Pointer over "green-activate"
    REQUIRE(toGreenToggle->value() == true);

    stateMachineInstance->pointerDown(rive::Vec2D(100.0f, 110.0f));
    // "gray-activate" is clicked
    REQUIRE(grayToggle->value() == true);
    // Pointer over "red-activate" and "green-activate", but "red-activate" is
    // opaque and above so green activate does not trigger
    REQUIRE(toGreenToggle->value() == false);
    delete stateMachineInstance;
}

TEST_CASE("hit test on opaque nested artboard", "[hittest]")
{
    // This artboard (300x300) has a main rect at [0, 0, 300, 300]
    // this rect has a listener that toggles "second-gray-toggle"
    // and a nested artboard at [0, 0, 150, 150]
    // the nested artboard and the rect have opaque targets
    auto file = ReadRiveFile("assets/opaque_hit_test.riv");

    auto artboard = file->artboard("second");
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("second-state-machine");

    REQUIRE(artboardInstance != nullptr);
    REQUIRE(artboardInstance->stateMachineCount() == 1);

    REQUIRE(stateMachine != nullptr);

    rive::StateMachineInstance* stateMachineInstance =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());

    auto nestedArtboard =
        stateMachineInstance->artboard()->find<rive::NestedArtboard>(
            "second-nested");
    REQUIRE(nestedArtboard != nullptr);
    auto nestedArtboardStateMachine =
        nestedArtboard->nestedAnimations()[0]->as<NestedStateMachine>();
    REQUIRE(nestedArtboardStateMachine != nullptr);
    auto nestedArtboardStateMachineInstance =
        nestedArtboardStateMachine->stateMachineInstance();

    auto secondNestedBoolTarget =
        nestedArtboardStateMachineInstance->getBool("bool-target");
    REQUIRE(secondNestedBoolTarget != nullptr);

    artboardInstance->advance(0.0f);
    stateMachineInstance->advanceAndApply(0.0f);

    REQUIRE(secondNestedBoolTarget->value() == false);

    auto secondGrayToggle = stateMachineInstance->getBool("second-gray-toggle");
    REQUIRE(secondGrayToggle != nullptr);

    stateMachineInstance->pointerDown(rive::Vec2D(100.0f, 250.0f));
    // toggle changes value because it is not under an opaque nested artboard
    REQUIRE(secondGrayToggle->value() == true);

    stateMachineInstance->pointerDown(rive::Vec2D(301.0f, 50.0f));
    // toggle does not change because it is beyond the area of the square by 1
    // pixel And the 2px padding is unly used after the coarse grained test
    // passes
    REQUIRE(secondGrayToggle->value() == true);

    stateMachineInstance->pointerDown(rive::Vec2D(100.0f, 50.0f));
    // toggle does not change because it is under an opaque nested artboard
    REQUIRE(secondGrayToggle->value() == true);

    // nested toggle changes because it's on top of shape
    REQUIRE(secondNestedBoolTarget->value() == true);

    // A timeline switches draw order and the nested artboard is now below the
    // rect
    stateMachineInstance->advanceAndApply(1.0f);
    stateMachineInstance->advance(0.0f);

    stateMachineInstance->pointerDown(rive::Vec2D(100.0f, 50.0f));
    // So now the pointer down is captured by the rect
    REQUIRE(secondGrayToggle->value() == false);

    // nested toggle does not change because it's below shape
    REQUIRE(secondNestedBoolTarget->value() == true);
    delete stateMachineInstance;
}

TEST_CASE("early out on listeners", "[hittest]")
{
    auto file = ReadRiveFile("assets/pointer_events.riv");

    auto artboard = file->artboard("art-1");
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("sm-1");

    REQUIRE(artboardInstance != nullptr);
    REQUIRE(artboardInstance->stateMachineCount() == 1);

    REQUIRE(stateMachine != nullptr);

    rive::StateMachineInstance* stateMachineInstance =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());

    stateMachineInstance->advance(0.0f);
    artboardInstance->advance(0.0f);
    REQUIRE(stateMachineInstance->needsAdvance() == true);
    stateMachineInstance->advance(0.0f);
    REQUIRE(stateMachineInstance->hitComponentsCount() == 4);
    // Hit component with only pointer down and pointer up listeners
    auto hitComponentWithEarlyOut = stateMachineInstance->hitComponent(0);
    // Hit component that can't early out because it has a pointer enter event
    auto hitComponentWithNoEarlyOut = stateMachineInstance->hitComponent(1);
    // Hit component that can't early out because it is an opaque target
    auto hitComponentOpaque = stateMachineInstance->hitComponent(2);
    // Hit component that can early out on all and pointer up
    auto hitComponentOnlyPointerDown = stateMachineInstance->hitComponent(3);
    REQUIRE(hitComponentWithEarlyOut->earlyOutCount == 0);
    REQUIRE(hitComponentWithNoEarlyOut->earlyOutCount == 0);
    REQUIRE(hitComponentOpaque->earlyOutCount == 0);
    REQUIRE(hitComponentOnlyPointerDown->earlyOutCount == 0);
    stateMachineInstance->pointerMove(rive::Vec2D(100.0f, 250.0f));
    REQUIRE(hitComponentWithEarlyOut->earlyOutCount == 1);
    REQUIRE(hitComponentWithNoEarlyOut->earlyOutCount == 0);
    REQUIRE(hitComponentOpaque->earlyOutCount == 0);
    REQUIRE(hitComponentOnlyPointerDown->earlyOutCount == 1);
    stateMachineInstance->pointerExit(rive::Vec2D(100.0f, 250.0f));
    REQUIRE(hitComponentWithEarlyOut->earlyOutCount == 2);
    REQUIRE(hitComponentWithNoEarlyOut->earlyOutCount == 0);
    REQUIRE(hitComponentOnlyPointerDown->earlyOutCount == 2);
    REQUIRE(hitComponentOpaque->earlyOutCount == 0);
    stateMachineInstance->pointerDown(rive::Vec2D(100.0f, 250.0f));
    REQUIRE(hitComponentWithEarlyOut->earlyOutCount == 2);
    REQUIRE(hitComponentWithNoEarlyOut->earlyOutCount == 0);
    REQUIRE(hitComponentOpaque->earlyOutCount == 0);
    REQUIRE(hitComponentOnlyPointerDown->earlyOutCount == 2);
    stateMachineInstance->pointerUp(rive::Vec2D(100.0f, 250.0f));
    REQUIRE(hitComponentWithEarlyOut->earlyOutCount == 2);
    REQUIRE(hitComponentWithNoEarlyOut->earlyOutCount == 0);
    REQUIRE(hitComponentOpaque->earlyOutCount == 0);
    REQUIRE(hitComponentOnlyPointerDown->earlyOutCount == 3);
    stateMachineInstance->pointerMove(rive::Vec2D(105.0f, 205.0f));
    REQUIRE(hitComponentWithEarlyOut->earlyOutCount == 3);
    REQUIRE(hitComponentWithNoEarlyOut->earlyOutCount == 0);
    REQUIRE(hitComponentOpaque->earlyOutCount == 0);
    REQUIRE(hitComponentOnlyPointerDown->earlyOutCount == 4);

    delete stateMachineInstance;
}

TEST_CASE("click event", "[hittest]")
{
    // This test has two rectangles of size [200, 200]
    // positioned at [100,100] and [200, 200]
    // they overlap between coordinates [100,100]-[200, 200]
    // they are inside a group that has a listener attached to it
    // that listener should fire an event on "Click"
    auto file = ReadRiveFile("assets/click_event.riv");

    auto artboard = file->artboard("art-1");
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("sm-1");

    REQUIRE(artboardInstance != nullptr);
    REQUIRE(artboardInstance->stateMachineCount() == 1);

    REQUIRE(stateMachine != nullptr);

    rive::StateMachineInstance* stateMachineInstance =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());

    stateMachineInstance->advance(0.0f);
    artboardInstance->advance(0.0f);
    REQUIRE(stateMachineInstance->needsAdvance() == true);
    stateMachineInstance->advance(0.0f);
    // There is a single listener with two shapes in it
    REQUIRE(stateMachineInstance->hitComponentsCount() == 2);
    auto layerCount = stateMachine->layerCount();
    REQUIRE(layerCount == 1);
    REQUIRE(stateMachineInstance->reportedEventCount() == 0);
    // Click in place should trigger a click event
    stateMachineInstance->pointerDown(rive::Vec2D(75.0f, 75.0f));
    stateMachineInstance->pointerUp(rive::Vec2D(75.0f, 75.0f));
    REQUIRE(stateMachineInstance->reportedEventCount() == 1);
    // Pointer down inside shape but Pointer up outside the shape
    // should not trigger a click event
    stateMachineInstance->pointerDown(rive::Vec2D(75.0f, 75.0f));
    stateMachineInstance->pointerUp(rive::Vec2D(300.0f, 75.0f));
    REQUIRE(stateMachineInstance->reportedEventCount() == 1);
    // Pointer down outside shape but Pointer up inside the shape
    // should not trigger a click event
    stateMachineInstance->pointerDown(rive::Vec2D(300.0f, 75.0f));
    stateMachineInstance->pointerUp(rive::Vec2D(75.0f, 75.0f));
    REQUIRE(stateMachineInstance->reportedEventCount() == 1);
    // Pointer down in shape 1 Pointer up in shape 2 of the same group
    // should trigger a click event
    stateMachineInstance->pointerDown(rive::Vec2D(75.0f, 75.0f));
    stateMachineInstance->pointerUp(rive::Vec2D(225.0f, 225.0f));
    REQUIRE(stateMachineInstance->reportedEventCount() == 2);
    // Pointer down and up in area where both shapes overlap
    // should trigger a single click event
    stateMachineInstance->pointerDown(rive::Vec2D(150.0f, 150.0f));
    stateMachineInstance->pointerUp(rive::Vec2D(150.0f, 150.0f));
    REQUIRE(stateMachineInstance->reportedEventCount() == 3);

    delete stateMachineInstance;
}

TEST_CASE("multiple shapes with mouse movement behavior", "[hittest]")
{
    // This test has two rectangles of size [200, 200]
    // positioned at [100,100] and [100, 200]
    // they overlap between coordinates [100,0]-[200, 200]
    // they are inside a group that has a Pointer enter and a Pointer out
    // listeners that toggle between two states (red and green)
    // starting at "red"
    auto file = ReadRiveFile("assets/click_event.riv");

    auto artboard = file->artboard("art-2");
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("sm-1");

    REQUIRE(artboardInstance != nullptr);
    REQUIRE(artboardInstance->stateMachineCount() == 1);

    REQUIRE(stateMachine != nullptr);

    rive::StateMachineInstance* stateMachineInstance =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());

    stateMachineInstance->advance(0.0f);
    artboardInstance->advance(0.0f);
    REQUIRE(stateMachineInstance->needsAdvance() == true);
    stateMachineInstance->advance(0.0f);
    // There is a single listener with two shapes in it
    REQUIRE(stateMachineInstance->hitComponentsCount() == 2);
    auto layerCount = stateMachine->layerCount();
    REQUIRE(layerCount == 1);
    // Move over the first shape
    stateMachineInstance->pointerMove(rive::Vec2D(75.0f, 75.0f));
    artboardInstance->advance(0.0f);
    stateMachineInstance->advanceAndApply(0.0f);

    {
        auto state = stateMachineInstance->layerState(0);
        REQUIRE(state->is<rive::AnimationState>());
        auto animation = state->as<rive::AnimationState>()->animation();
        REQUIRE(animation->name() == "green");
    }
    // Move over the second shape, nothing should change
    stateMachineInstance->pointerMove(rive::Vec2D(200.0f, 75.0f));
    artboardInstance->advance(0.0f);
    stateMachineInstance->advanceAndApply(0.0f);

    {
        auto state = stateMachineInstance->layerState(0);
        REQUIRE(state->is<rive::AnimationState>());
        auto animation = state->as<rive::AnimationState>()->animation();
        REQUIRE(animation->name() == "green");
    }
    // Move out of the second shape, should go back to red
    stateMachineInstance->pointerMove(rive::Vec2D(400.0f, 75.0f));
    artboardInstance->advance(0.0f);
    stateMachineInstance->advanceAndApply(0.0f);

    {
        auto state = stateMachineInstance->layerState(0);
        REQUIRE(state->is<rive::AnimationState>());
        auto animation = state->as<rive::AnimationState>()->animation();
        REQUIRE(animation->name() == "red");
    }
    // Move back into the second shape, should go to green
    stateMachineInstance->pointerMove(rive::Vec2D(200.0f, 75.0f));
    artboardInstance->advance(0.0f);
    stateMachineInstance->advanceAndApply(0.0f);

    {
        auto state = stateMachineInstance->layerState(0);
        REQUIRE(state->is<rive::AnimationState>());
        auto animation = state->as<rive::AnimationState>()->animation();
        REQUIRE(animation->name() == "green");
    }

    delete stateMachineInstance;
}
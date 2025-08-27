/*
 * Copyright 2022 Rive
 */

#include <rive/math/aabb.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/animation/state_machine_input_instance.hpp>
#include <rive/animation/nested_state_machine.hpp>
#include <rive/shapes/ellipse.hpp>
#include <rive/shapes/shape.hpp>
#include "rive_file_reader.hpp"

#include <catch.hpp>
#include <cstdio>

using namespace rive;

TEST_CASE("align target with preserve offset off test", "[listener_align]")
{
    // The circle starts at coords 100, 100
    // Once the pointer move has acted, the new coords should be 100, 51
    auto file = ReadRiveFile("assets/align_target.riv");

    auto artboard = file->artboard("preserve-inactive");
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("align-state-machine");

    REQUIRE(artboardInstance != nullptr);
    REQUIRE(artboardInstance->stateMachineCount() == 1);

    REQUIRE(stateMachine != nullptr);

    rive::StateMachineInstance* stateMachineInstance =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());

    artboardInstance->advance(0.0f);
    stateMachineInstance->advanceAndApply(0.0f);
    stateMachineInstance->advance(0.0f);
    auto circle = stateMachineInstance->artboard()->find<rive::Shape>("circle");
    REQUIRE(circle != nullptr);
    stateMachineInstance->pointerMove(rive::Vec2D(100.0f, 50.0f));
    stateMachineInstance->pointerMove(rive::Vec2D(100.0f, 51.0f));
    stateMachineInstance->advanceAndApply(1.0f);
    stateMachineInstance->advance(0.0f);
    REQUIRE(circle->x() == 100.0f);
    REQUIRE(circle->y() == 51.0f);
    delete stateMachineInstance;
}

TEST_CASE("align target preserve offset test", "[listener_align]")
{
    // The circle starts at coords 100, 100
    // Once the pointer move has acted, the new coords should be 100, 101
    auto file = ReadRiveFile("assets/align_target.riv");

    auto artboard = file->artboard("preserve-active");
    auto artboardInstance = artboard->instance();
    auto stateMachine = artboard->stateMachine("align-state-machine");

    REQUIRE(artboardInstance != nullptr);
    REQUIRE(artboardInstance->stateMachineCount() == 1);

    REQUIRE(stateMachine != nullptr);

    rive::StateMachineInstance* stateMachineInstance =
        new rive::StateMachineInstance(stateMachine, artboardInstance.get());

    artboardInstance->advance(0.0f);
    stateMachineInstance->advanceAndApply(0.0f);
    stateMachineInstance->advance(0.0f);
    auto circle = stateMachineInstance->artboard()->find<rive::Shape>("circle");
    REQUIRE(circle != nullptr);
    stateMachineInstance->pointerMove(rive::Vec2D(100.0f, 50.0f));
    stateMachineInstance->pointerMove(rive::Vec2D(100.0f, 51.0f));
    stateMachineInstance->advanceAndApply(1.0f);
    stateMachineInstance->advance(0.0f);
    REQUIRE(circle->x() == 100.0f);
    REQUIRE(circle->y() == 101.0f);
    delete stateMachineInstance;
}
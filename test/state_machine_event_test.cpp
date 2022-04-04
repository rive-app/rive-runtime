#include <rive/core/binary_reader.hpp>
#include <rive/file.hpp>
#include <rive/animation/state_machine_bool.hpp>
#include <rive/animation/state_machine_layer.hpp>
#include <rive/animation/state_machine_event.hpp>
#include <rive/animation/animation_state.hpp>
#include <rive/animation/entry_state.hpp>
#include <rive/animation/state_transition.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/animation/state_machine_input_instance.hpp>
#include <rive/animation/blend_state_1d.hpp>
#include <rive/animation/blend_animation_1d.hpp>
#include <rive/animation/blend_state_direct.hpp>
#include <rive/animation/blend_state_transition.hpp>
#include <rive/animation/event_input_change.hpp>
#include <rive/node.hpp>
#include "catch.hpp"
#include "rive_file_reader.hpp"
#include <cstdio>

TEST_CASE("file with state machine events be read", "[file]") {
    RiveFileReader reader("../../test/assets/bullet_man.riv");

    auto artboard = reader.file()->artboard("Bullet Man");
    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->stateMachineCount() == 1);

    auto stateMachine = artboard->stateMachine(0);
    REQUIRE(stateMachine != nullptr);

    REQUIRE(stateMachine->eventCount() == 3);
    REQUIRE(stateMachine->inputCount() == 4);

    // Expect each of the three events to have one input change each.
    auto event1 = stateMachine->event(0);
    auto target1 = artboard->resolve(event1->targetId());
    REQUIRE(target1->is<rive::Node>());
    REQUIRE(target1->as<rive::Node>()->name() == "HandWickHit");
    REQUIRE(event1->inputChangeCount() == 1);
    auto inputChange1 = event1->inputChange(0);
    REQUIRE(inputChange1 != nullptr);
    REQUIRE(inputChange1->inputId() == 0);

    auto event2 = stateMachine->event(1);
    auto target2 = artboard->resolve(event2->targetId());
    REQUIRE(target2->is<rive::Node>());
    REQUIRE(target2->as<rive::Node>()->name() == "HandCannonHit");
    REQUIRE(event2->inputChangeCount() == 1);
    auto inputChange2 = event2->inputChange(0);
    REQUIRE(inputChange2 != nullptr);
    REQUIRE(inputChange2->inputId() == 1);

    auto event3 = stateMachine->event(2);
    auto target3 = artboard->resolve(event3->targetId());
    REQUIRE(target3->is<rive::Node>());
    REQUIRE(target3->as<rive::Node>()->name() == "HandHelmetHit");
    REQUIRE(event3->inputChangeCount() == 1);
    auto inputChange3 = event3->inputChange(0);
    REQUIRE(inputChange3 != nullptr);
    REQUIRE(inputChange3->inputId() == 2);
}

TEST_CASE("hit testing via a state machine works", "[file]") {
    RiveFileReader reader("../../test/assets/bullet_man.riv");

    auto artboard = reader.file()->artboard("Bullet Man")->instance();
    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->stateMachineCount() == 1);

    auto stateMachine = artboard->stateMachineInstance(0);
    REQUIRE(stateMachine != nullptr);
    // Advance artboard once so design time state is effectively in the transforms.
    artboard->advance(0.0f);
    stateMachine->advance(0.0f);
    // Don't advance artboard again after applying state machine or our pointerDown will be off. The
    // coordinates used in this test were from the design-time state.

    auto trigger = stateMachine->getTrigger("Light");
    REQUIRE(trigger != nullptr);
    stateMachine->pointerDown(rive::Vec2D(71.0f, 263.0f));

    REQUIRE(trigger->didFire());
}

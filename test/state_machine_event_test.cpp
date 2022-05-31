#include <rive/core/binary_reader.hpp>
#include <rive/file.hpp>
#include <rive/animation/state_machine_bool.hpp>
#include <rive/animation/state_machine_layer.hpp>
#include <rive/animation/state_machine_listener.hpp>
#include <rive/animation/animation_state.hpp>
#include <rive/animation/entry_state.hpp>
#include <rive/animation/state_transition.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/animation/state_machine_input_instance.hpp>
#include <rive/animation/blend_state_1d.hpp>
#include <rive/animation/blend_animation_1d.hpp>
#include <rive/animation/blend_state_direct.hpp>
#include <rive/animation/blend_state_transition.hpp>
#include <rive/animation/listener_input_change.hpp>
#include <rive/node.hpp>
#include "catch.hpp"
#include "rive_file_reader.hpp"
#include <cstdio>

TEST_CASE("file with state machine listeners be read", "[file]") {
    auto file = ReadRiveFile("../../test/assets/bullet_man.riv");

    auto artboard = file->artboard("Bullet Man");
    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->stateMachineCount() == 1);

    auto stateMachine = artboard->stateMachine(0);
    REQUIRE(stateMachine != nullptr);

    REQUIRE(stateMachine->listenerCount() == 3);
    REQUIRE(stateMachine->inputCount() == 4);

    // Expect each of the three listeners to have one input change each.
    auto listener1 = stateMachine->listener(0);
    auto target1 = artboard->resolve(listener1->targetId());
    REQUIRE(target1->is<rive::Node>());
    REQUIRE(target1->as<rive::Node>()->name() == "HandWickHit");
    REQUIRE(listener1->inputChangeCount() == 1);
    auto inputChange1 = listener1->inputChange(0);
    REQUIRE(inputChange1 != nullptr);
    REQUIRE(inputChange1->inputId() == 0);

    auto listener2 = stateMachine->listener(1);
    auto target2 = artboard->resolve(listener2->targetId());
    REQUIRE(target2->is<rive::Node>());
    REQUIRE(target2->as<rive::Node>()->name() == "HandCannonHit");
    REQUIRE(listener2->inputChangeCount() == 1);
    auto inputChange2 = listener2->inputChange(0);
    REQUIRE(inputChange2 != nullptr);
    REQUIRE(inputChange2->inputId() == 1);

    auto listener3 = stateMachine->listener(2);
    auto target3 = artboard->resolve(listener3->targetId());
    REQUIRE(target3->is<rive::Node>());
    REQUIRE(target3->as<rive::Node>()->name() == "HandHelmetHit");
    REQUIRE(listener3->inputChangeCount() == 1);
    auto inputChange3 = listener3->inputChange(0);
    REQUIRE(inputChange3 != nullptr);
    REQUIRE(inputChange3->inputId() == 2);
}

TEST_CASE("hit testing via a state machine works", "[file]") {
    auto file = ReadRiveFile("../../test/assets/bullet_man.riv");

    auto artboard = file->artboard("Bullet Man")->instance();
    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->stateMachineCount() == 1);

    auto stateMachine = artboard->stateMachineAt(0);
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

TEST_CASE("hit a toggle boolean listener", "[file]") {
    auto file = ReadRiveFile("../../test/assets/light_switch.riv");

    auto artboard = file->artboard()->instance();
    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->stateMachineCount() == 1);

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);

    artboard->advance(0.0f);
    stateMachine->advance(0.0f);

    auto switchButton = stateMachine->getBool("On");
    REQUIRE(switchButton != nullptr);
    REQUIRE(switchButton->value() == true);

    stateMachine->pointerDown(rive::Vec2D(150.0f, 258.0f));
    stateMachine->pointerUp(rive::Vec2D(150.0f, 258.0f));
    // Got toggled off after pressing
    REQUIRE(switchButton->value() == false);

    stateMachine->pointerDown(rive::Vec2D(150.0f, 258.0f));
    stateMachine->pointerUp(rive::Vec2D(150.0f, 258.0f));
    // Got toggled back on after pressing
    REQUIRE(switchButton->value() == true);
}

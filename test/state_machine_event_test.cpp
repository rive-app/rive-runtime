#include <rive/core/binary_reader.hpp>
#include <rive/file.hpp>
#include <rive/animation/state_machine_bool.hpp>
#include <rive/animation/state_machine_layer.hpp>
#include <rive/animation/animation_state.hpp>
#include <rive/animation/entry_state.hpp>
#include <rive/animation/state_transition.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/animation/state_machine_input_instance.hpp>
#include <rive/animation/blend_state_1d.hpp>
#include <rive/animation/blend_animation_1d.hpp>
#include <rive/animation/blend_state_direct.hpp>
#include <rive/animation/blend_state_transition.hpp>
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
}

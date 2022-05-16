#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/shapes/clipping_shape.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include "no_op_factory.hpp"
#include "no_op_renderer.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("default state machine is detected at load", "[file]") {
    auto file = ReadRiveFile("../../test/assets/entry.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->defaultStateMachine() != nullptr);
    REQUIRE(artboard->defaultStateMachine()->name() == "State Machine 1");

    auto artboardInstance = artboard->instance();
    REQUIRE(artboardInstance != nullptr);
    auto defaultStateMachineInstance = artboardInstance->defaultStateMachineInstance();

    REQUIRE(defaultStateMachineInstance != nullptr);
    REQUIRE(defaultStateMachineInstance->name() == "State Machine 1");
}

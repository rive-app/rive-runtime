#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/shapes/clipping_shape.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include "utils/no_op_factory.hpp"
#include "utils/no_op_renderer.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("default state machine is detected at load", "[file]")
{
    auto file = ReadRiveFile("../../test/assets/entry.riv");

    auto abi = file->artboardAt(0);
    auto index = abi->defaultStateMachineIndex();

    REQUIRE(index >= 0);
    REQUIRE(abi->stateMachineNameAt(index) == "State Machine 1");

    auto smi = abi->defaultStateMachine();

    REQUIRE(smi != nullptr);
    REQUIRE(smi->name() == "State Machine 1");

    // default scene is the same as the default statemachine (when we have one)
    auto scene = abi->defaultScene();
    REQUIRE(scene != nullptr);
    REQUIRE(scene->name() == smi->name());
}

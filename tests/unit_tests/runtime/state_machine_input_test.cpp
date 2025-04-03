#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/nested_artboard.hpp>
#include <rive/shapes/clipping_shape.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/animation/nested_simple_animation.hpp>
#include <rive/animation/nested_state_machine.hpp>
#include <rive/animation/nested_bool.hpp>
#include <rive/animation/nested_trigger.hpp>
#include <rive/animation/nested_number.hpp>
#include <rive/relative_local_asset_loader.hpp>
#include <utils/no_op_factory.hpp>
#include <utils/no_op_renderer.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("File with state machine inputs loads", "[state_machine]")
{
    // created by test in rive_core.
    auto file = ReadRiveFile("assets/smi_test.riv");

    auto nestedArtboard = file->artboard()->find<rive::NestedArtboard>(
        "artboard to nest component");
    REQUIRE(nestedArtboard != nullptr);
    REQUIRE(nestedArtboard->x() == 100);
    REQUIRE(nestedArtboard->y() == 100);
    REQUIRE(nestedArtboard->name() == "artboard to nest component");
    REQUIRE(nestedArtboard->artboardId() == 1);

    auto nestedSimpleAnimation =
        file->artboard()->find<rive::NestedStateMachine>("");
    REQUIRE(nestedSimpleAnimation != nullptr);
    REQUIRE(nestedSimpleAnimation->name() == "");
    REQUIRE(nestedSimpleAnimation->animationId() == 0);

    auto nestedTrigger = file->artboard()->find<rive::NestedTrigger>("");
    REQUIRE(nestedTrigger != nullptr);
    REQUIRE(nestedTrigger->name() == "");
    REQUIRE(nestedTrigger->inputId() == 0);

    auto nestedBool = file->artboard()->find<rive::NestedBool>("");
    REQUIRE(nestedBool != nullptr);
    REQUIRE(nestedBool->name() == "");
    REQUIRE(nestedBool->inputId() == 1);

    auto nestedNumber = file->artboard()->find<rive::NestedNumber>("");
    REQUIRE(nestedNumber != nullptr);
    REQUIRE(nestedNumber->name() == "");
    REQUIRE(nestedNumber->inputId() == 2);
}

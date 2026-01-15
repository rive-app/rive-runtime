
#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/viewmodel/viewmodel_instance_string.hpp"
#include "rive_file_reader.hpp"

using namespace rive;

TEST_CASE("scripted listener action", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/scripted_listener_action.riv", &silver);
    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());
    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createViewModelInstance(artboard.get());
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    silver.addFrame();

    stateMachine->pointerDown(rive::Vec2D(200.0f, 20.0f), 1);
    stateMachine->pointerUp(rive::Vec2D(200.0f, 20.0f), 1);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    stateMachine->pointerDown(rive::Vec2D(300.0f, 20.0f), 2);
    stateMachine->pointerUp(rive::Vec2D(300.0f, 20.0f), 2);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    stateMachine->pointerDown(rive::Vec2D(400.0f, 20.0f), 3);
    stateMachine->pointerUp(rive::Vec2D(400.0f, 20.0f), 3);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("scripted_listener_action"));
}
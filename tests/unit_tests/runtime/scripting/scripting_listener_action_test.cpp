
#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/viewmodel/viewmodel_instance_string.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
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

TEST_CASE("Listener action inputs", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/listener_action_inputs.riv", &silver);

    auto artboard = file->artboardDefault();
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    auto renderer = silver.makeRenderer();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    stateMachine->pointerDown(
        rive::Vec2D(artboard->width() / 2.0f, artboard->height() / 2.0f),
        3);
    stateMachine->pointerUp(
        rive::Vec2D(artboard->width() / 2.0f, artboard->height() / 2.0f),
        3);
    silver.addFrame();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("listener_action_inputs"));
}

TEST_CASE("Listener action script receives pointer types and the data",
          "[silver]")
{
    auto file = ReadRiveFile("assets/scripted_listener_context.riv");

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createViewModelInstance(artboard.get());
    auto keyString =
        vmi->propertyValue("keyInput")->as<ViewModelInstanceString>();
    auto pointerType =
        vmi->propertyValue("pointerType")->as<ViewModelInstanceString>();
    auto stringInput =
        vmi->propertyValue("stringInput")->as<ViewModelInstanceString>();
    auto posX = vmi->propertyValue("posX")->as<ViewModelInstanceNumber>();
    auto posY = vmi->propertyValue("posY")->as<ViewModelInstanceNumber>();
    auto isFocus =
        vmi->propertyValue("isFocus")->as<ViewModelInstanceBoolean>();
    auto eventReported =
        vmi->propertyValue("eventReported")->as<ViewModelInstanceBoolean>();
    auto viewModelChanged =
        vmi->propertyValue("viewModelChanged")->as<ViewModelInstanceBoolean>();
    REQUIRE(keyString->propertyValue() == "");
    REQUIRE(pointerType->propertyValue() == "");
    REQUIRE(stringInput->propertyValue() == "");
    REQUIRE(posX->propertyValue() == 0);
    REQUIRE(posY->propertyValue() == 0);
    REQUIRE(isFocus->propertyValue() == false);
    REQUIRE(eventReported->propertyValue() == false);
    REQUIRE(viewModelChanged->propertyValue() == false);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.016f);

    stateMachine->pointerMove(Vec2D(200.0f, 210.0f));
    stateMachine->advanceAndApply(0.016f);

    REQUIRE(keyString->propertyValue() == "");
    REQUIRE(pointerType->propertyValue() == "pointerEnter");
    REQUIRE(stringInput->propertyValue() == "");
    REQUIRE(posX->propertyValue() == 200.0f);
    REQUIRE(posY->propertyValue() == 210.0f);
    REQUIRE(isFocus->propertyValue() == false);
    ///
    stateMachine->pointerDown(Vec2D(250.0f, 251.0f));
    stateMachine->pointerUp(Vec2D(250.0f, 251.0f));
    stateMachine->advanceAndApply(0.016f);

    REQUIRE(keyString->propertyValue() == "");
    REQUIRE(pointerType->propertyValue() == "click");
    REQUIRE(stringInput->propertyValue() == "");
    REQUIRE(posX->propertyValue() == 250.0f);
    REQUIRE(posY->propertyValue() == 251.0f);
    REQUIRE(isFocus->propertyValue() == false);

    auto focusManager = artboard->focusManager();
    focusManager->focusNext();
    stateMachine->advanceAndApply(0.016f);

    REQUIRE(keyString->propertyValue() == "");
    REQUIRE(pointerType->propertyValue() == "click");
    REQUIRE(stringInput->propertyValue() == "");
    REQUIRE(posX->propertyValue() == 250.0f);
    REQUIRE(posY->propertyValue() == 251.0f);
    REQUIRE(isFocus->propertyValue() == true);
    focusManager->keyInput(rive::Key::a,
                           rive::KeyModifiers::none,
                           false,
                           false);
    stateMachine->advanceAndApply(0.016f);

    REQUIRE(keyString->propertyValue() ==
            "65, no shift, no meta, no control, no alt, phase: up");
    REQUIRE(pointerType->propertyValue() == "click");
    REQUIRE(stringInput->propertyValue() == "");
    REQUIRE(posX->propertyValue() == 250.0f);
    REQUIRE(posY->propertyValue() == 251.0f);
    REQUIRE(isFocus->propertyValue() == true);
    focusManager->textInput("With text input");
    stateMachine->advanceAndApply(0.016f);

    REQUIRE(keyString->propertyValue() ==
            "65, no shift, no meta, no control, no alt, phase: up");
    REQUIRE(pointerType->propertyValue() == "click");
    REQUIRE(stringInput->propertyValue() == "With text input");
    REQUIRE(posX->propertyValue() == 250.0f);
    REQUIRE(posY->propertyValue() == 251.0f);
    REQUIRE(isFocus->propertyValue() == true);
    focusManager->keyInput(rive::Key::b,
                           rive::KeyModifiers::meta | rive::KeyModifiers::shift,
                           true,
                           false);
    stateMachine->advanceAndApply(0.016f);
    REQUIRE(keyString->propertyValue() ==
            "66, with shift, with meta, no control, no alt, phase: down");
    REQUIRE(pointerType->propertyValue() == "click");
    REQUIRE(stringInput->propertyValue() == "With text input");
    REQUIRE(posX->propertyValue() == 250.0f);
    REQUIRE(posY->propertyValue() == 251.0f);
    REQUIRE(isFocus->propertyValue() == true);

    focusManager->focusNext();
    stateMachine->advanceAndApply(0.016f);
    REQUIRE(keyString->propertyValue() ==
            "66, with shift, with meta, no control, no alt, phase: down");
    REQUIRE(pointerType->propertyValue() == "click");
    REQUIRE(stringInput->propertyValue() == "With text input");
    REQUIRE(posX->propertyValue() == 250.0f);
    REQUIRE(posY->propertyValue() == 251.0f);
    REQUIRE(isFocus->propertyValue() == false);
    focusManager->keyInput(rive::Key::a,
                           rive::KeyModifiers::none,
                           false,
                           false);
    stateMachine->advanceAndApply(0.016f);
    REQUIRE(keyString->propertyValue() ==
            "66, with shift, with meta, no control, no alt, phase: down");
    REQUIRE(pointerType->propertyValue() == "click");
    REQUIRE(stringInput->propertyValue() == "With text input");
    REQUIRE(posX->propertyValue() == 250.0f);
    REQUIRE(posY->propertyValue() == 251.0f);
    REQUIRE(isFocus->propertyValue() == false);
    REQUIRE(eventReported->propertyValue() == true);
    REQUIRE(viewModelChanged->propertyValue() == true);
}
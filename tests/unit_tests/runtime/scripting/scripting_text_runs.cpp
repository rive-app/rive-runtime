
#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive_file_reader.hpp"

using namespace rive;

TEST_CASE("script creates view models that map to text runs", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/script_create_text_runs.riv", &silver);
    auto artboard = file->artboardNamed("main");

    silver.frameSize(artboard->width(), artboard->height());
    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    // Push element
    {
        silver.addFrame();
        rive::ViewModelInstanceViewModel* button =
            vmi->propertyValue("newButton")
                ->as<rive::ViewModelInstanceViewModel>();
        REQUIRE(button != nullptr);
        auto instance = button->referenceViewModelInstance();
        rive::ViewModelInstanceTrigger* trigger =
            instance->propertyValue("onClick")
                ->as<rive::ViewModelInstanceTrigger>();
        REQUIRE(trigger != nullptr);
        trigger->trigger();
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
    }

    // Push element at specific index
    {
        silver.addFrame();
        rive::ViewModelInstanceViewModel* button =
            vmi->propertyValue("newAtButton")
                ->as<rive::ViewModelInstanceViewModel>();
        REQUIRE(button != nullptr);
        auto instance = button->referenceViewModelInstance();
        rive::ViewModelInstanceTrigger* trigger =
            instance->propertyValue("onClick")
                ->as<rive::ViewModelInstanceTrigger>();
        REQUIRE(trigger != nullptr);
        trigger->trigger();
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
    }

    // Swap elements from indexes
    {
        silver.addFrame();
        rive::ViewModelInstanceViewModel* button =
            vmi->propertyValue("swapButton")
                ->as<rive::ViewModelInstanceViewModel>();
        REQUIRE(button != nullptr);
        auto instance = button->referenceViewModelInstance();
        rive::ViewModelInstanceTrigger* trigger =
            instance->propertyValue("onClick")
                ->as<rive::ViewModelInstanceTrigger>();
        REQUIRE(trigger != nullptr);
        trigger->trigger();
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
    }

    // Shift first element
    {
        silver.addFrame();
        rive::ViewModelInstanceViewModel* button =
            vmi->propertyValue("shiftButton")
                ->as<rive::ViewModelInstanceViewModel>();
        REQUIRE(button != nullptr);
        auto instance = button->referenceViewModelInstance();
        rive::ViewModelInstanceTrigger* trigger =
            instance->propertyValue("onClick")
                ->as<rive::ViewModelInstanceTrigger>();
        REQUIRE(trigger != nullptr);
        trigger->trigger();
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
    }

    // Pop last element
    {
        silver.addFrame();
        rive::ViewModelInstanceViewModel* button =
            vmi->propertyValue("popButton")
                ->as<rive::ViewModelInstanceViewModel>();
        REQUIRE(button != nullptr);
        auto instance = button->referenceViewModelInstance();
        rive::ViewModelInstanceTrigger* trigger =
            instance->propertyValue("onClick")
                ->as<rive::ViewModelInstanceTrigger>();
        REQUIRE(trigger != nullptr);
        trigger->trigger();
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
    }

    // Pop all elements and pop beyond empty list
    {
        silver.addFrame();
        rive::ViewModelInstanceViewModel* button =
            vmi->propertyValue("popButton")
                ->as<rive::ViewModelInstanceViewModel>();
        REQUIRE(button != nullptr);
        auto instance = button->referenceViewModelInstance();
        rive::ViewModelInstanceTrigger* trigger =
            instance->propertyValue("onClick")
                ->as<rive::ViewModelInstanceTrigger>();
        REQUIRE(trigger != nullptr);
        trigger->trigger();
        trigger->trigger();
        trigger->trigger();
        trigger->trigger();
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
    }

    // Push 2 elements
    {
        silver.addFrame();
        rive::ViewModelInstanceViewModel* button =
            vmi->propertyValue("newButton")
                ->as<rive::ViewModelInstanceViewModel>();
        REQUIRE(button != nullptr);
        auto instance = button->referenceViewModelInstance();
        rive::ViewModelInstanceTrigger* trigger =
            instance->propertyValue("onClick")
                ->as<rive::ViewModelInstanceTrigger>();
        REQUIRE(trigger != nullptr);
        trigger->trigger();
        trigger->trigger();
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("script_create_text_runs"));
}


#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive_file_reader.hpp"

using namespace rive;

TEST_CASE("Scripted Context markNeedsUpdate works", "[scripting]")
{
    ScriptingTest vm(
        R"(

-- Called once when the script initializes.
function init(self: MyNode, context: Context): boolean
  context:markNeedsUpdate()
  return true
end

)");
    ScriptedObjectTest scriptedObjectTest;
    lua_State* L = vm.state();
    auto top = lua_gettop(L);
    {
        lua_getglobal(L, "init");
        lua_pushvalue(L, -2);
        lua_newrive<ScriptedContext>(L, &scriptedObjectTest);
        CHECK(lua_pcall(L, 2, 1, 0) == LUA_OK);
        rive_lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
        CHECK(scriptedObjectTest.needsUpdate());
    }
}

TEST_CASE("script has access to user created view models via Data", "[silver]")
{
    rive::SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/script_create_viewmodel_instance.riv", &silver);
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

    CHECK(silver.matches("script_create_viewmodel_instance"));
}

TEST_CASE("script has access to the data bound view model", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/viewmodel_from_context.riv", &silver);
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
    silver.addFrame();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("viewmodel_from_context"));
}

#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <utils/no_op_renderer.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <rive/shapes/paint/color.hpp>
#include <rive/shapes/paint/fill.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <rive/text/text_value_run.hpp>
#include <rive/custom_property_number.hpp>
#include <rive/custom_property_string.hpp>
#include <rive/custom_property_boolean.hpp>
#include <rive/constraints/follow_path_constraint.hpp>
#include <rive/viewmodel/viewmodel_instance_number.hpp>
#include <rive/viewmodel/viewmodel_instance_color.hpp>
#include <rive/viewmodel/viewmodel_instance_string.hpp>
#include <rive/viewmodel/viewmodel_instance_boolean.hpp>
#include <rive/viewmodel/viewmodel_instance_enum.hpp>
#include <rive/viewmodel/viewmodel_instance_trigger.hpp>
#include <rive/viewmodel/viewmodel_instance_list.hpp>
#include <rive/viewmodel/viewmodel_instance_list_item.hpp>
#include "rive/animation/state_machine_instance.hpp"
#include "rive/nested_artboard.hpp"
#include "utils/serializing_factory.hpp"
#include "rive_file_reader.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include <catch.hpp>
#include <cstdio>

using namespace rive;

TEST_CASE("scripted properties can be passed to luau", "[scripting_properties]")
{
    auto file = ReadRiveFile("assets/data_binding_test.riv");

    auto artboard = file->artboard("artboard-1")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);
    artboard->advance(0.0f);

    CHECK(viewModelInstance->viewModel()->name() == "vm1");
    // View model properties
    // Number
    auto widthProperty = viewModelInstance->propertyValue("width");
    REQUIRE(widthProperty != nullptr);
    REQUIRE(widthProperty->is<rive::ViewModelInstanceNumber>());
    // Number with comverter
    auto rotationProperty = viewModelInstance->propertyValue("rotation");
    REQUIRE(rotationProperty != nullptr);
    REQUIRE(rotationProperty->is<rive::ViewModelInstanceNumber>());
    // Color
    auto colorProperty = viewModelInstance->propertyValue("color");
    REQUIRE(colorProperty != nullptr);
    REQUIRE(colorProperty->is<rive::ViewModelInstanceColor>());
    // String
    auto textProperty = viewModelInstance->propertyValue("text");
    REQUIRE(textProperty != nullptr);
    REQUIRE(textProperty->is<rive::ViewModelInstanceString>());
    // Boolean
    auto orientProperty = viewModelInstance->propertyValue("orient");
    REQUIRE(orientProperty != nullptr);
    REQUIRE(orientProperty->is<rive::ViewModelInstanceBoolean>());
    // Update view model values
    widthProperty->as<rive::ViewModelInstanceNumber>()->propertyValue(200.0f);
    rotationProperty->as<rive::ViewModelInstanceNumber>()->propertyValue(
        180.0f);
    colorProperty->as<rive::ViewModelInstanceColor>()->propertyValue(
        rive::colorARGB(255, 0, 255, 0));
    textProperty->as<rive::ViewModelInstanceString>()->propertyValue(
        "New text");
    orientProperty->as<rive::ViewModelInstanceBoolean>()->propertyValue(true);

    auto artboardWithTrigger = file->artboard("artboard-2")->instance();
    REQUIRE(artboardWithTrigger != nullptr);
    auto viewModelInstanceWithTrigger =
        file->createDefaultViewModelInstance(artboardWithTrigger.get());
    REQUIRE(viewModelInstanceWithTrigger != nullptr);
    auto triggerProperty =
        viewModelInstanceWithTrigger->propertyValue("trigger-prop");
    REQUIRE(triggerProperty != nullptr);
    REQUIRE(triggerProperty->is<rive::ViewModelInstanceTrigger>());

    ScriptingTest vm(
        R"TEST_SRC(
type ViewModel = {
    getNumber: (self, name:string)->Property<number>?,
    getTrigger: (self, name:string)->PropertyTrigger?
}
type Vm1 = {
    width: Property<number>,
    rotation: Property<rotation>,
    color: Property<color>,
    text: Property<string>,
    orient: Property<boolean>,
    getNumber: (self, name:string)->Property<number>?,
    -- todo: addListener
}
local data:Vm1?
local triggerProp:PropertyTrigger?
local calledChange:boolean = false
local calledChangeWithContext:boolean = false

function provide(vm:Vm1, vm2:ViewModel)
    triggerProp = vm2:getTrigger('trigger-prop')
    if triggerProp then
        print("trigger is good")
        triggerProp:addListener(vm2, triggerTriggered)
    else
        print("bad trigger")
    end
    data = vm
    data2 = vm2
    data.rotation:addListener(changed)
    data.rotation:addListener(vm, changedWithContext)
    print("data provided")
end

function triggerTriggered(context:ViewModel)
    print("trigger was triggered!")
end

function changedWithContext(context:Vm1)
    print("changed with context")
    calledChangeWithContext = true
end

function changed()
    print("changed")
    calledChange = true
end

function getRotation():number
    if data then
        return data.rotation.value
    end
    return 0
end
function getRotationByName():number
    if data then
        local rotation = data:getNumber('rotation')
        if rotation then
            return rotation.value
        end
    end
    return 0
end

function calledBoth():boolean
    return calledChange and calledChangeWithContext
end

function callTriggerIndirectly()
    if triggerProp then
        triggerProp:fire()
    end
end
)TEST_SRC");
    auto L = vm.state();
    lua_getglobal(L, "provide");
    lua_newrive<ScriptedViewModel>(L,
                                   L,
                                   ref_rcp(viewModelInstance->viewModel()),
                                   viewModelInstance);
    lua_newrive<ScriptedViewModel>(
        L,
        L,
        ref_rcp(viewModelInstanceWithTrigger->viewModel()),
        viewModelInstanceWithTrigger);
    CHECK(lua_pcall(L, 2, 0, 0) == LUA_OK);
    CHECK(vm.console.size() == 2);
    CHECK(vm.console[0] == "trigger is good");
    CHECK(vm.console[1] == "data provided");

    lua_getglobal(L, "getRotation");
    lua_pcall(L, 0, 1, 0);
    REQUIRE(luaL_checknumber(L, -1) == Approx(180.0f));
    lua_pop(L, 1);

    lua_getglobal(L, "getRotationByName");
    lua_pcall(L, 0, 1, 0);
    REQUIRE(luaL_checknumber(L, -1) == Approx(180.0f));
    lua_pop(L, 1);

    lua_getglobal(L, "calledBoth");
    lua_pcall(L, 0, 1, 0);
    CHECK(lua_toboolean(L, -1) == 0);
    lua_pop(L, 1);

    // Change the rotation value
    rotationProperty->as<rive::ViewModelInstanceNumber>()->propertyValue(
        360.0f);

    // Expect calledBoth to now return true.
    lua_getglobal(L, "calledBoth");
    lua_pcall(L, 0, 1, 0);
    CHECK(lua_toboolean(L, -1) == 1);
    lua_pop(L, 1);

    triggerProperty->as<rive::ViewModelInstanceTrigger>()->trigger();

    // Check print statements.
    CHECK(vm.console.size() == 5);
    CHECK(vm.console[0] == "trigger is good");
    CHECK(vm.console[1] == "data provided");
    CHECK(vm.console[2] == "changed");
    CHECK(vm.console[3] == "changed with context");
    CHECK(vm.console[4] == "trigger was triggered!");

    lua_getglobal(L, "callTriggerIndirectly");
    lua_pcall(L, 0, 0, 0);
    // another print
    CHECK(vm.console.size() == 6);
    CHECK(vm.console[5] == "trigger was triggered!");
}

TEST_CASE("scripted list can be passed to luau", "[scripting_properties]")
{
    auto file = ReadRiveFile("assets/scripted_graph.riv");

    auto artboard = file->artboard("Artboard")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);
    artboard->advance(0.0f);

    CHECK(viewModelInstance->viewModel()->name() == "Graph");
    // View model properties
    // Number
    auto pointsProperty = viewModelInstance->propertyValue("points");
    REQUIRE(pointsProperty != nullptr);
    REQUIRE(pointsProperty->is<rive::ViewModelInstanceList>());

    ScriptingTest vm(
        R"TEST_SRC(
function provide(vm)
        local points = vm.points
        print(`length is {points.length}`)
        for i = 1, points.length, 1 do
            print(`point is {points[i].x.value} {points[i].y.value}`)
        end
        -- add a listener to the point at x == 2
        local p = points[3]
        p.y:addListener(p, yChanged);
end

function iterateAgain(vm)
        local points = vm.points
        print(`length is {points.length}`)
        for i = 1, points.length, 1 do
            print(`point is {points[i].x.value} {points[i].y.value}`)
        end
end

function yChanged(point)
    if point then
        print(`y changed to: {point.y.value}`)
    end
end
)TEST_SRC");
    auto L = vm.state();

    lua_newrive<ScriptedViewModel>(L,
                                   L,
                                   ref_rcp(viewModelInstance->viewModel()),
                                   viewModelInstance);
    lua_getglobal(L, "provide");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 0, 0) == LUA_OK);
    auto list = pointsProperty->as<ViewModelInstanceList>();
    list->removeItem(1);

    auto item = list->listItems()[1]; // 2 is now at 1
    auto vmi = item->viewModelInstance();
    auto yProperty = vmi->propertyValue("y");
    CHECK(yProperty->is<rive::ViewModelInstanceNumber>());
    yProperty->as<rive::ViewModelInstanceNumber>()->propertyValue(22);

    lua_getglobal(L, "iterateAgain");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 0, 0) == LUA_OK);
    yProperty->as<rive::ViewModelInstanceNumber>()->propertyValue(23);

    CHECK(vm.console.size() == 13);
    CHECK(vm.console[0] == "length is 5");
    CHECK(vm.console[1] == "point is 0 10");
    CHECK(vm.console[2] == "point is 1 15");
    CHECK(vm.console[3] == "point is 2 16");
    CHECK(vm.console[4] == "point is 3 18");
    CHECK(vm.console[5] == "point is 4 9");
    CHECK(vm.console[6] == "y changed to: 22");
    CHECK(vm.console[7] == "length is 4");
    CHECK(vm.console[8] == "point is 0 10");
    CHECK(vm.console[9] == "point is 2 22");
    CHECK(vm.console[10] == "point is 3 18");
    CHECK(vm.console[11] == "point is 4 9");
    CHECK(vm.console[12] == "y changed to: 23");
}

TEST_CASE("scripted color can be passed to luau", "[scripting_properties]")
{
    auto file = ReadRiveFile("assets/scripted_color.riv");

    auto artboard = file->artboard("ColorArtboard")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);
    artboard->advance(0.0f);

    CHECK(viewModelInstance->viewModel()->name() == "colorsVm");
    auto property = viewModelInstance->propertyValue("colorProp");
    REQUIRE(property != nullptr);
    REQUIRE(property->is<rive::ViewModelInstanceColor>());
    auto colorProperty = property->as<rive::ViewModelInstanceColor>();

    ScriptingTest vm(
        R"TEST_SRC(
function init(vm)
    print(`color init to {vm.colorProp.value}`)
    vm.colorProp:addListener(vm.colorProp, colorChanged)
end

function setRed(vm)
    vm.colorProp.value = Color.rgb(255, 0, 0)
    print(`color is {vm.colorProp.value}`)
end

function colorChanged(color)
    print(`color changed to {color.value}`)
end

)TEST_SRC");
    auto L = vm.state();

    lua_newrive<ScriptedViewModel>(L,
                                   L,
                                   ref_rcp(viewModelInstance->viewModel()),
                                   viewModelInstance);
    lua_getglobal(L, "init");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 0, 0) == LUA_OK);
    CHECK(colorProperty->propertyValue() == 0xFF101566);

    colorProperty->propertyValue(0xFF101567);

    lua_getglobal(L, "setRed");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 0, 0) == LUA_OK);

    CHECK(vm.console.size() == 4);
    CHECK(vm.console[0] == ("color init to " + std::to_string(0xff101566)));
    CHECK(vm.console[1] == ("color changed to " + std::to_string(0xff101567)));
    CHECK(vm.console[2] == ("color changed to " + std::to_string(0xffff0000)));
    CHECK(vm.console[3] == ("color is " + std::to_string(0xffff0000)));
}

TEST_CASE("scripted string can be passed to luau", "[scripting_properties]")
{
    auto file = ReadRiveFile("assets/scripted_string.riv");

    auto artboard = file->artboard("StringArtboard")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);
    artboard->advance(0.0f);

    CHECK(viewModelInstance->viewModel()->name() == "stringVm");
    auto property = viewModelInstance->propertyValue("stringProp");
    REQUIRE(property != nullptr);
    REQUIRE(property->is<rive::ViewModelInstanceString>());
    auto stringProperty = property->as<rive::ViewModelInstanceString>();

    ScriptingTest vm(
        R"TEST_SRC(
function init(vm)
    print(`string init to {vm.stringProp.value}`)
    vm.stringProp:addListener(vm.stringProp, stringChanged)
end

function setHello(vm)
    vm.stringProp.value = "Hello World"
    print(`string is {vm.stringProp.value}`)
end

function stringChanged(str)
    print(`string changed to {str.value}`)
end

)TEST_SRC");
    auto L = vm.state();

    lua_newrive<ScriptedViewModel>(L,
                                   L,
                                   ref_rcp(viewModelInstance->viewModel()),
                                   viewModelInstance);
    lua_getglobal(L, "init");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 0, 0) == LUA_OK);
    CHECK(stringProperty->propertyValue() == "yo");

    stringProperty->propertyValue("yoo");

    lua_getglobal(L, "setHello");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 0, 0) == LUA_OK);

    CHECK(vm.console.size() == 4);
    CHECK(vm.console[0] == "string init to yo");
    CHECK(vm.console[1] == "string changed to yoo");
    CHECK(vm.console[2] == "string changed to Hello World");
    CHECK(vm.console[3] == "string is Hello World");
}

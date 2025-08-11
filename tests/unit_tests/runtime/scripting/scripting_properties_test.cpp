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

    ScriptingTest vm(
        R"TEST_SRC(
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
local calledChange:boolean = false
local calledChangeWithContext:boolean = false

function provide(vm:Vm1)
    data = vm
    data.rotation:addListener(changed)
    data.rotation:addListener(vm, changedWithContext)
end

function changedWithContext(context:Vm1)
    calledChangeWithContext = true
end

function changed() 
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
)TEST_SRC");
    auto L = vm.state();
    lua_getglobal(L, "provide");
    auto data = make_rcp<ViewModelInstanceViewModel>();
    data->referenceViewModelInstance(viewModelInstance);
    lua_newrive<ScriptedPropertyViewModel>(
        L,
        L,
        ref_rcp(viewModelInstance->viewModel()),
        data);
    lua_pcall(L, 1, 0, 0);
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
}
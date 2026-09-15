#include <catch.hpp>
#include "scripting_test_utilities.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_string.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"
#include "rive/viewmodel/viewmodel_property_string.hpp"
#include "rive/viewmodel/viewmodel_property_viewmodel.hpp"

using namespace rive;

// A nested view model property read from Luau must pair the referenced
// instance with the referenced view model, not the owner's: instance() on the
// nested value mints the nested type. Pairing it with the owner made scripts
// mint owner-typed instances, which recursed when pushed into a bound list.
TEST_CASE("nested view model property mints the referenced type", "[scripting]")
{
    ScriptingTest test(
        R"(
function probe(owner: ViewModel): ViewModel?
  local prop = owner:getViewModel('proto')
  if prop == nil then
    return nil
  end
  return prop.value
end
)");
    lua_State* L = test.state();

    auto item = rcp<ViewModel>(new ViewModel());
    item->name("Item");
    auto* itemString = new ViewModelPropertyString();
    itemString->name("itemOnly");
    item->addProperty(itemString);

    auto owner = rcp<ViewModel>(new ViewModel());
    owner->name("Owner");
    auto* ownerString = new ViewModelPropertyString();
    ownerString->name("ownerOnly");
    owner->addProperty(ownerString);
    auto* protoProperty = new ViewModelPropertyViewModel();
    protoProperty->name("proto");
    owner->addProperty(protoProperty);

    auto itemInstance = make_rcp<ViewModelInstance>();
    itemInstance->viewModel(item.get());
    auto* itemValue = new ViewModelInstanceString();
    itemValue->viewModelProperty(itemString);
    itemInstance->addValue(itemValue);

    auto ownerInstance = make_rcp<ViewModelInstance>();
    ownerInstance->viewModel(owner.get());
    auto* ownerValue = new ViewModelInstanceString();
    ownerValue->viewModelProperty(ownerString);
    ownerInstance->addValue(ownerValue);
    auto* protoValue = new ViewModelInstanceViewModel();
    protoValue->viewModelProperty(protoProperty);
    protoValue->parentViewModelInstance(ownerInstance.get());
    protoValue->referenceViewModelInstance(itemInstance);
    ownerInstance->addValue(protoValue);

    auto top = lua_gettop(L);
    lua_getglobal(L, "probe");
    lua_newrive<ScriptedViewModel>(L, L, owner, ownerInstance);
    REQUIRE(lua_pcall(L, 1, 1, 0) == LUA_OK);
    auto* nested = lua_torive<ScriptedViewModel>(L, -1);
    REQUIRE(nested != nullptr);
    // instance() mints from the wrapper's view model, so the nested value
    // must pair the referenced type, not the owner's.
    CHECK(nested->viewModel().get() == item.get());
    CHECK(nested->viewModelInstance().get() == itemInstance.get());
    lua_pop(L, 1);
    CHECK(top == lua_gettop(L));
}

// A property first read while its reference is unset must pick up the
// referenced type once the reference binds and the wrapper relinks.
TEST_CASE("nested view model value refreshes after the reference relinks",
          "[scripting]")
{
    ScriptingTest test(
        R"(
function getProp(owner: ViewModel): PropertyViewModel?
  return owner:getViewModel('proto')
end

function getValue(prop: PropertyViewModel): ViewModel?
  return prop.value
end
)");
    lua_State* L = test.state();

    auto item = rcp<ViewModel>(new ViewModel());
    item->name("Item");
    auto* itemString = new ViewModelPropertyString();
    itemString->name("itemOnly");
    item->addProperty(itemString);

    auto owner = rcp<ViewModel>(new ViewModel());
    owner->name("Owner");
    auto* protoProperty = new ViewModelPropertyViewModel();
    protoProperty->name("proto");
    owner->addProperty(protoProperty);

    auto itemInstance = make_rcp<ViewModelInstance>();
    itemInstance->viewModel(item.get());

    auto ownerInstance = make_rcp<ViewModelInstance>();
    ownerInstance->viewModel(owner.get());
    auto* protoValue = new ViewModelInstanceViewModel();
    protoValue->viewModelProperty(protoProperty);
    protoValue->parentViewModelInstance(ownerInstance.get());
    ownerInstance->addValue(protoValue);

    auto top = lua_gettop(L);
    lua_getglobal(L, "getProp");
    lua_newrive<ScriptedViewModel>(L, L, owner, ownerInstance);
    REQUIRE(lua_pcall(L, 1, 1, 0) == LUA_OK);
    auto* prop = lua_torive<ScriptedPropertyViewModel>(L, -1);
    REQUIRE(prop != nullptr);
    int propRef = lua_ref(L, -1);
    lua_pop(L, 1);

    // First read caches a value with no referenced type.
    lua_getglobal(L, "getValue");
    lua_rawgeti(L, LUA_REGISTRYINDEX, propRef);
    REQUIRE(lua_pcall(L, 1, 1, 0) == LUA_OK);
    auto* before = lua_torive<ScriptedViewModel>(L, -1);
    REQUIRE(before != nullptr);
    CHECK(before->viewModelInstance() == nullptr);
    lua_pop(L, 1);

    // The reference binds later; a relink must refresh the pairing.
    protoValue->referenceViewModelInstance(itemInstance);
    prop->relinkDataBind();

    lua_getglobal(L, "getValue");
    lua_rawgeti(L, LUA_REGISTRYINDEX, propRef);
    REQUIRE(lua_pcall(L, 1, 1, 0) == LUA_OK);
    auto* after = lua_torive<ScriptedViewModel>(L, -1);
    REQUIRE(after != nullptr);
    CHECK(after->viewModel().get() == item.get());
    CHECK(after->viewModelInstance().get() == itemInstance.get());
    lua_pop(L, 1);

    lua_unref(L, propRef);
    CHECK(top == lua_gettop(L));
}

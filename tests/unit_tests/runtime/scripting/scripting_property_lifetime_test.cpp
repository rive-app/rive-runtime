#include <catch.hpp>
#include "scripting_test_utilities.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"
#include "rive/viewmodel/viewmodel_property_boolean.hpp"
#include "rive/viewmodel/viewmodel_property_number.hpp"
#include "rive/viewmodel/viewmodel_property_viewmodel.hpp"
#include "rive/data_bind/data_context.hpp"

using namespace rive;

namespace
{
// Mirrors the shape of a real listener action script: resolve once in init,
// stash the Property wrappers on self, mutate them on every invocation.
constexpr const char* kCachingScript = R"(
local cached = {}

function init(screen: ViewModel)
  cached.counter = screen:getNumber('DelayCounter')
  cached.enabled = screen:getBoolean('DelayEnabled')
end

function bump()
  cached.enabled.value = true
  cached.counter.value += 1
  return cached.counter.value, cached.enabled.value
end

function probe(name: string)
  return cached[name]
end
)";

struct Screen
{
    rcp<ViewModel> viewModel;
    rcp<ViewModelInstance> instance;
    ViewModelInstanceNumber* counter = nullptr;
    ViewModelInstanceBoolean* enabled = nullptr;
};

Screen makeScreen(ViewModelPropertyNumber* counterProperty,
                  ViewModelPropertyBoolean* enabledProperty,
                  ViewModel* screenViewModel)
{
    Screen screen;
    screen.viewModel = ref_rcp(screenViewModel);
    screen.instance = make_rcp<ViewModelInstance>();
    screen.instance->viewModel(screenViewModel);

    screen.counter = new ViewModelInstanceNumber();
    screen.counter->viewModelProperty(counterProperty);
    screen.instance->addValue(screen.counter);

    screen.enabled = new ViewModelInstanceBoolean();
    screen.enabled->viewModelProperty(enabledProperty);
    screen.instance->addValue(screen.enabled);

    return screen;
}

// Calls bump() and returns the (counter, enabled) pair the script reads back
// immediately after writing them.
void callBump(lua_State* L, float* counterOut, bool* enabledOut)
{
    lua_getglobal(L, "bump");
    REQUIRE(lua_pcall(L, 0, 2, 0) == LUA_OK);
    *counterOut = (float)lua_tonumber(L, -2);
    *enabledOut = lua_toboolean(L, -1) != 0;
    lua_pop(L, 2);
}
} // namespace

// Swapping a nested view model reference does not rewire wrappers a script
// already resolved. The swapped-in view model is a different set of properties,
// so a script opts into it by resolving again. What the old wrappers must not
// do is rot: the instance they came from stays alive through m_owningInstance,
// so they keep reading and writing a coherent instance instead of an orphan
// whose owner was freed underneath them.
TEST_CASE("a cached property keeps its own view model alive across a swap",
          "[scripting_property_lifetime]")
{
    ScriptingTest test(R"(
local cached = {}

function init(screens: ViewModel)
  local prop = screens:getViewModel('screen')
  local screen = prop.value
  cached.counter = screen:getNumber('DelayCounter')
  cached.enabled = screen:getBoolean('DelayEnabled')
end

function bump()
  cached.enabled.value = true
  cached.counter.value += 1
  return cached.counter.value, cached.enabled.value
end

-- What a script does to pick up a swapped-in view model.
function reresolve(screens: ViewModel)
  local prop = screens:getViewModel('screen')
  local screen = prop.value
  cached.counter = screen:getNumber('DelayCounter')
  cached.enabled = screen:getBoolean('DelayEnabled')
end
)");
    lua_State* L = test.state();

    auto screenViewModel = rcp<ViewModel>(new ViewModel());
    screenViewModel->name("Screen");
    auto* counterProperty = new ViewModelPropertyNumber();
    counterProperty->name("DelayCounter");
    screenViewModel->addProperty(counterProperty);
    auto* enabledProperty = new ViewModelPropertyBoolean();
    enabledProperty->name("DelayEnabled");
    screenViewModel->addProperty(enabledProperty);

    auto screensViewModel = rcp<ViewModel>(new ViewModel());
    screensViewModel->name("Screens");
    auto* screenProperty = new ViewModelPropertyViewModel();
    screenProperty->name("screen");
    screensViewModel->addProperty(screenProperty);

    auto screensInstance = make_rcp<ViewModelInstance>();
    screensInstance->viewModel(screensViewModel.get());
    auto* screenValue = new ViewModelInstanceViewModel();
    screenValue->viewModelProperty(screenProperty);
    screenValue->parentViewModelInstance(screensInstance.get());
    screensInstance->addValue(screenValue);

    auto first =
        makeScreen(counterProperty, enabledProperty, screenViewModel.get());
    auto second =
        makeScreen(counterProperty, enabledProperty, screenViewModel.get());
    screenValue->referenceViewModelInstance(first.instance);

    // Hold the first screen only through the script, so the swap below is the
    // last thing releasing it apart from the wrappers themselves.
    ViewModelInstance* firstRaw = first.instance.get();
    ViewModelInstanceNumber* firstCounter = first.counter;

    auto top = lua_gettop(L);

    lua_newrive<ScriptedViewModel>(L, L, screensViewModel, screensInstance);
    int screensWrapperRef = lua_ref(L, -1);
    lua_pop(L, 1);

    lua_getglobal(L, "init");
    lua_rawgeti(L, LUA_REGISTRYINDEX, screensWrapperRef);
    REQUIRE(lua_pcall(L, 1, 0, 0) == LUA_OK);

    float counter = 0.0f;
    bool enabled = false;
    callBump(L, &counter, &enabled);
    CHECK(firstCounter->propertyValue() == 1.0f);

    screensInstance->replaceViewModelByProperty(screenValue, second.instance);
    first.instance = nullptr;
    lua_gc(L, LUA_GCCOLLECT, 0);

    // The wrappers still address the screen they were resolved from, and it is
    // still alive: the write lands and reads back.
    callBump(L, &counter, &enabled);
    CHECK(counter == 2.0f);
    CHECK(enabled);
    CHECK(firstCounter->propertyValue() == 2.0f);
    CHECK(firstCounter->viewModelInstance() == firstRaw);
    // Nothing was rewired onto the swapped-in screen behind the script's back.
    CHECK(second.counter->propertyValue() == 0.0f);

    // Resolving again is how a script moves to the new screen.
    lua_getglobal(L, "reresolve");
    lua_rawgeti(L, LUA_REGISTRYINDEX, screensWrapperRef);
    REQUIRE(lua_pcall(L, 1, 0, 0) == LUA_OK);
    callBump(L, &counter, &enabled);
    CHECK(second.counter->propertyValue() == 1.0f);
    CHECK(second.enabled->propertyValue());
    CHECK(firstCounter->propertyValue() == 2.0f);

    lua_unref(L, screensWrapperRef);
    CHECK(top == lua_gettop(L));
}

// Properties a script resolves in init must belong to the scripted object that
// ran init, not to the context's orphan list. Ownership is what ties their
// lifetime to that object's own re-init; an orphan is swept wholesale by
// File::setScriptingVM, which in the editor fires on every runtime-file
// regeneration and wipes wrappers that are still in use.
TEST_CASE("init-time properties belong to the scripted object",
          "[scripting_property_lifetime]")
{
    ScriptingTest test(R"(
function init(self, context: Context): boolean
  local vm = context:viewModel()
  if vm == nil then
    return false
  end
  self.counter = vm:getNumber('DelayCounter')
  self.enabled = vm:getBoolean('DelayEnabled')
  return true
end

return function()
  return { init = init, counter = nil, enabled = nil }
end
)");
    lua_State* L = test.state();
    REQUIRE(lua_type(L, -1) == LUA_TFUNCTION);

    auto screenViewModel = rcp<ViewModel>(new ViewModel());
    screenViewModel->name("Screen");
    auto* counterProperty = new ViewModelPropertyNumber();
    counterProperty->name("DelayCounter");
    screenViewModel->addProperty(counterProperty);
    auto* enabledProperty = new ViewModelPropertyBoolean();
    enabledProperty->name("DelayEnabled");
    screenViewModel->addProperty(enabledProperty);

    auto screen =
        makeScreen(counterProperty, enabledProperty, screenViewModel.get());

    ScriptedObjectTest object;
    object.implementedMethods(object.implementedMethods() | (1 << 9));
    object.dataContext(make_rcp<DataContext>(screen.instance));

    REQUIRE(object.ScriptedObject::ensureScriptInitialized(test.vm(),
                                                           refTopFunction(L)));
    REQUIRE(object.ScriptedObject::hydrateScriptInputs());

    CHECK(object.trackedScriptedProperties().size() == 2);
    for (auto* property : object.trackedScriptedProperties())
    {
        CHECK(property->owner() == &object);
    }
}

// The editor pcalls a script's init from Dart, so the properties it resolves
// have no C++ owner and land on the context's orphan list. A host claims them
// with an owner tag instead. File::setScriptingVM / cleanupScriptingVM reach
// the untagged sweep on every runtime-file regeneration, which must leave
// claimed properties alone -- otherwise it wipes wrappers a running script is
// still holding.
TEST_CASE("the untagged orphan sweep spares claimed properties",
          "[scripting_property_lifetime]")
{
    ScriptingTest test(kCachingScript);
    lua_State* L = test.state();
    auto* context = static_cast<ScriptingContext*>(lua_getthreaddata(L));
    REQUIRE(context != nullptr);

    auto screenViewModel = rcp<ViewModel>(new ViewModel());
    screenViewModel->name("Screen");
    auto* counterProperty = new ViewModelPropertyNumber();
    counterProperty->name("DelayCounter");
    screenViewModel->addProperty(counterProperty);
    auto* enabledProperty = new ViewModelPropertyBoolean();
    enabledProperty->name("DelayEnabled");
    screenViewModel->addProperty(enabledProperty);

    auto screen =
        makeScreen(counterProperty, enabledProperty, screenViewModel.get());

    auto top = lua_gettop(L);

    lua_newrive<ScriptedViewModel>(L, L, screenViewModel, screen.instance);
    int screenWrapperRef = lua_ref(L, -1);
    lua_pop(L, 1);

    // A host claims everything init resolves, the way the editor's Dart
    // scripted objects do around their init pcall.
    context->orphanOwnerTag(7);
    lua_getglobal(L, "init");
    lua_rawgeti(L, LUA_REGISTRYINDEX, screenWrapperRef);
    REQUIRE(lua_pcall(L, 1, 0, 0) == LUA_OK);
    context->orphanOwnerTag(0);

    float counter = 0.0f;
    bool enabled = false;
    callBump(L, &counter, &enabled);
    CHECK(counter == 1.0f);

    // A File dropping its scripting VM sweeps unclaimed orphans only.
    context->disposeOrphanScriptedProperties();

    callBump(L, &counter, &enabled);
    CHECK(counter == 2.0f);
    CHECK(enabled);
    CHECK(screen.counter->propertyValue() == 2.0f);

    // The host releasing its tag does dispose them.
    context->disposeOrphanScriptedProperties(uint32_t{7});
    callBump(L, &counter, &enabled);
    CHECK(counter == 0.0f);

    lua_unref(L, screenWrapperRef);
    CHECK(top == lua_gettop(L));
}

// The invariant that makes ownership safe: two scripted objects resolving the
// same names get their own wrappers, because each resolves through its own
// Context (ScriptedContext caches per object, and the editor mints a fresh
// wrapper per pushDataContextViewModel call). Nothing hands one object's
// wrapper to another, so one object's teardown cannot detach a wrapper another
// is using. If a shared path is ever introduced, this fails and the
// first-minter-owns model has to become co-ownership.
TEST_CASE("scripted objects do not share property wrappers",
          "[scripting_property_lifetime]")
{
    ScriptingTest test(R"(
function init(self, context: Context): boolean
  local vm = context:viewModel()
  if vm == nil then
    return false
  end
  self.counter = vm:getNumber('DelayCounter')
  return true
end

return function()
  return { init = init, counter = nil }
end
)");
    lua_State* L = test.state();
    REQUIRE(lua_type(L, -1) == LUA_TFUNCTION);
    int generator = refTopFunction(L);

    auto screenViewModel = rcp<ViewModel>(new ViewModel());
    screenViewModel->name("Screen");
    auto* counterProperty = new ViewModelPropertyNumber();
    counterProperty->name("DelayCounter");
    screenViewModel->addProperty(counterProperty);
    auto* enabledProperty = new ViewModelPropertyBoolean();
    enabledProperty->name("DelayEnabled");
    screenViewModel->addProperty(enabledProperty);

    auto screen =
        makeScreen(counterProperty, enabledProperty, screenViewModel.get());
    auto dataContext = make_rcp<DataContext>(screen.instance);

    auto first = std::make_unique<ScriptedObjectTest>();
    first->implementedMethods(first->implementedMethods() | (1 << 9));
    first->dataContext(dataContext);
    REQUIRE(
        first->ScriptedObject::ensureScriptInitialized(test.vm(), generator));
    REQUIRE(first->ScriptedObject::hydrateScriptInputs());
    REQUIRE(first->trackedScriptedProperties().size() == 1);
    auto* firstCounter = first->trackedScriptedProperties()[0];

    auto second = std::make_unique<ScriptedObjectTest>();
    second->implementedMethods(second->implementedMethods() | (1 << 9));
    second->dataContext(dataContext);
    REQUIRE(
        second->ScriptedObject::ensureScriptInitialized(test.vm(), generator));
    REQUIRE(second->ScriptedObject::hydrateScriptInputs());
    REQUIRE(second->trackedScriptedProperties().size() == 1);
    auto* secondCounter = second->trackedScriptedProperties()[0];

    CHECK(firstCounter != secondCounter);
    CHECK(firstCounter->instanceValue() == secondCounter->instanceValue());

    // One session ending leaves the other's wrapper attached and usable.
    first.reset();
    CHECK(secondCounter->disposed() == false);
    REQUIRE(secondCounter->instanceValue() != nullptr);
    static_cast<ScriptedPropertyNumber*>(secondCounter)->setValue(4.0f);
    CHECK(screen.counter->propertyValue() == 4.0f);

    lua_unref(L, generator);
}

// A ScriptedViewModel hands back the wrapper it cached for a name, and re-mints
// when that wrapper was disposed out from under it. The disposed check reads
// the userdata tag, and ScriptedPropertyViewModel's was missing from the set --
// so a nested view model property, alone among the property types, came back
// dead: pushValue answered an empty wrapper, and everything resolved through it
// resolved against nothing.
TEST_CASE("a disposed nested view model property is re-minted",
          "[scripting_property_lifetime]")
{
    ScriptingTest test(R"(
function resolve(owner: ViewModel)
  return owner:getViewModel('screen')
end
)");
    lua_State* L = test.state();

    auto screenViewModel = rcp<ViewModel>(new ViewModel());
    screenViewModel->name("Screen");
    auto* counterProperty = new ViewModelPropertyNumber();
    counterProperty->name("DelayCounter");
    screenViewModel->addProperty(counterProperty);
    auto* enabledProperty = new ViewModelPropertyBoolean();
    enabledProperty->name("DelayEnabled");
    screenViewModel->addProperty(enabledProperty);

    auto ownerViewModel = rcp<ViewModel>(new ViewModel());
    ownerViewModel->name("Screens");
    auto* screenProperty = new ViewModelPropertyViewModel();
    screenProperty->name("screen");
    ownerViewModel->addProperty(screenProperty);

    auto ownerInstance = make_rcp<ViewModelInstance>();
    ownerInstance->viewModel(ownerViewModel.get());
    auto* screenValue = new ViewModelInstanceViewModel();
    screenValue->viewModelProperty(screenProperty);
    screenValue->parentViewModelInstance(ownerInstance.get());
    ownerInstance->addValue(screenValue);

    auto screen =
        makeScreen(counterProperty, enabledProperty, screenViewModel.get());
    screenValue->referenceViewModelInstance(screen.instance);

    auto top = lua_gettop(L);

    lua_newrive<ScriptedViewModel>(L, L, ownerViewModel, ownerInstance);
    int ownerRef = lua_ref(L, -1);
    lua_pop(L, 1);

    lua_getglobal(L, "resolve");
    lua_rawgeti(L, LUA_REGISTRYINDEX, ownerRef);
    REQUIRE(lua_pcall(L, 1, 1, 0) == LUA_OK);
    auto* first = lua_torive<ScriptedPropertyViewModel>(L, -1);
    REQUIRE(first != nullptr);
    lua_pop(L, 1);

    // A sweep takes it while the view model that cached it stays alive.
    first->dispose();
    REQUIRE(first->disposed());

    lua_getglobal(L, "resolve");
    lua_rawgeti(L, LUA_REGISTRYINDEX, ownerRef);
    REQUIRE(lua_pcall(L, 1, 1, 0) == LUA_OK);
    auto* second = lua_torive<ScriptedPropertyViewModel>(L, -1);
    REQUIRE(second != nullptr);
    CHECK_FALSE(second->disposed());
    CHECK(second->instanceValue() == screenValue);
    lua_pop(L, 1);

    lua_unref(L, ownerRef);
    CHECK(top == lua_gettop(L));
}

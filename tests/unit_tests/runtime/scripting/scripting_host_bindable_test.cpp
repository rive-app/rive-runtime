// context:bindableArtboard: a script binds one of its own file's artboards
// to an artboard property, with no decoded file and no networking build.
#ifdef WITH_RIVE_SCRIPTING

#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive_file_reader.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/bindable_artboard.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance_artboard.hpp"
#include "utils/serializing_factory.hpp"

#include <string>

using namespace rive;

namespace
{
// The default artboard's view model has an artboard property `ab`, bound to
// a nested artboard, and the file holds the component artboards ch1 and ch2.
constexpr const char* kAsset = "assets/data_binding_artboards_test.riv";

// A scripted object of the host file, the way a script inside it has one.
class HostObject : public ScriptedObjectTest
{
public:
  explicit HostObject(File* file) : m_asset(make_rcp<ScriptAsset>())
  {
    m_asset->file(file);
    setAsset(m_asset);
  }

private:
  rcp<ScriptAsset> m_asset;
};

constexpr const char* kScript = R"(
function bindable(context: Context, name: string)
    local b = if name == "" then context:bindableArtboard()
        else context:bindableArtboard(name)
    if b == nil then return "nil" end
    return b.name .. (if b.data == nil then " nodata" else " data")
end

function assign(context: Context, host, name: string)
    local prop = host:getArtboard("ab")
    local before = if prop.value == nil then "nil" else prop.value.name
    prop.value = context:bindableArtboard(name)
    local after = if prop.value == nil then "nil" else prop.value.name
    return before .. "," .. after
end
)";

std::string popString(lua_State* L)
{
  const char* value = lua_tostring(L, -1);
  std::string result = value != nullptr ? value : "<nil>";
  lua_pop(L, 1);
  return result;
}

// Calls global `name` with a live Context for `object`, then the host view
// model when given, then `arg`.
std::string call(
    ScriptingTest& test,
    ScriptedObject* object,
    const char* name,
    ViewModelInstance* host,
    const char* arg)
{
  lua_State* L = test.state();
  lua_getglobal(L, name);
  lua_newrive<ScriptedContext>(L, object);
  int args = 1;
  if (host != nullptr)
  {
    lua_newrive<ScriptedViewModel>(
        L,
        L,
        ref_rcp(host->viewModel()),
        ref_rcp(host));
    args++;
  }
  lua_pushstring(L, arg);
  args++;
  if (lua_pcall(L, args, 1, 0) != LUA_OK)
  {
    std::string error = popString(L);
    FAIL(error);
  }
  return popString(L);
}

struct Host
{
  SerializingFactory factory;
  rcp<File> file;
  std::unique_ptr<ArtboardInstance> artboard;
  std::unique_ptr<StateMachineInstance> stateMachine;
  rcp<ViewModelInstance> viewModel;
  ScriptingTest test;
  HostObject object;

  Host()
      : file(ReadRiveFile(kAsset, &factory))
      , artboard(file->artboardDefault())
      , stateMachine(artboard->stateMachineAt(0))
      , viewModel(file->createViewModelInstance(artboard.get()))
      , test(kScript, 1, false, {}, true, &factory)
      , object(file.get())
  {
    stateMachine->bindViewModelInstance(viewModel);
    stateMachine->advanceAndApply(0.1f);
  }

  ViewModelInstanceArtboard* property()
  {
    return viewModel->propertyValue("ab")->as<ViewModelInstanceArtboard>();
  }

  // Whether a nested artboard of the host shows an artboard named `name`.
  bool shows(const char* name)
  {
    for (auto nested : artboard->find<NestedArtboard>())
    {
      auto instance = nested->artboardInstance();
      if (instance != nullptr && instance->name() == name)
      {
        return true;
      }
    }
    return false;
  }
};
} // namespace

TEST_CASE(
    "context:bindableArtboard answers the file's own artboards",
    "[scripting][host_bindable]")
{
  Host host;
  CHECK(
      call(host.test, &host.object, "bindable", nullptr, "ch1") ==
      "ch1 nodata");
  CHECK(
      call(host.test, &host.object, "bindable", nullptr, "ch2") ==
      "ch2 nodata");
  CHECK(call(host.test, &host.object, "bindable", nullptr, "nope") == "nil");
  // The default artboard, by its name.
  CHECK(
      call(host.test, &host.object, "bindable", nullptr, "") ==
      host.file->artboard()->name() + " nodata");
}

TEST_CASE(
    "a host bindable assigned from a script mounts in the bound slot",
    "[scripting][host_bindable]")
{
  Host host;
  auto property = host.property();
  REQUIRE(property != nullptr);
  // Authored in the editor: an artboard id, not a bindable.
  CHECK(property->asset() == nullptr);

  CHECK(
      call(host.test, &host.object, "assign", host.viewModel.get(), "ch1") ==
      "nil,ch1");
  REQUIRE(property->asset() != nullptr);
  CHECK(property->asset()->source()->name() == "ch1");
  // No instance of its own: the host makes one when it mounts it, so the
  // lookup ran no script init. Pins no file, so the host sees one of its
  // own, and brings no view model, so the host binds its own data as it
  // does for an authored id.
  CHECK(property->asset()->artboard() == nullptr);
  CHECK(property->asset()->file() == nullptr);
  CHECK(property->boundViewModelInstance() == nullptr);

  host.stateMachine->advanceAndApply(0.1f);
  CHECK(host.shows("ch1"));

  CHECK(
      call(host.test, &host.object, "assign", host.viewModel.get(), "ch2") ==
      "ch1,ch2");
  host.stateMachine->advanceAndApply(0.1f);
  CHECK(host.shows("ch2"));
  CHECK_FALSE(host.shows("ch1"));

  // An unknown name clears the property, as assigning nil does.
  CHECK(
      call(host.test, &host.object, "assign", host.viewModel.get(), "nope") ==
      "ch2,nil");
  CHECK(property->asset() == nullptr);
}

TEST_CASE(
    "context:bindableArtboard is nil for an object without a file",
    "[scripting][host_bindable]")
{
  ScriptingTest test(kScript, 1, false, {}, true);
  ScriptedObjectTest object;
  CHECK(call(test, &object, "bindable", nullptr, "ch1") == "nil");
}

#endif

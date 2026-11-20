#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/artboard.hpp"
#include "rive/transform_component.hpp"
#include "rive/scripted/scripted_read_tracker.hpp"
#include "rive_file_reader.hpp"
#include <memory>
#include <string>

using namespace rive;

namespace
{
// A scripted object that is one of an artboard's components.
class ComponentScriptedObject : public ScriptedObject
{
public:
  explicit ComponentScriptedObject(Component* component)
      : m_component(component)
  {}

  bool addScriptedDirt(ComponentDirt value, bool recurse = false) override
  {
    dirt |= value;
    return true;
  }
  uint32_t assetId() override { return 0; }
  ScriptProtocol scriptProtocol() override { return ScriptProtocol::utility; }
  void markNeedsUpdate() override {}
  Component* component() override { return m_component; }

  void readsWokeAdvance() override { advanceWakes++; }
  ComponentDirt dirt = ComponentDirt::None;
  int advanceWakes = 0;

private:
  Component* m_component;
};

const char* kScript = R"(
local kept = nil
local keptView = nil

function muzzleX(context)
  return context:artboard():node('muzzle').x
end

function setMuzzleX(context, x)
  context:artboard():node('muzzle').x = x
end

function sameWrappers(context)
  return rawequal(context:artboard(), context:artboard()),
    rawequal(context:node(), context:node())
end

function viewSize(context)
  local artboard = context:artboard()
  return artboard.width, artboard.height
end

function topLevel(context)
  local names = {}
  for _, child in context:artboard().children do
    table.insert(names, child.name)
  end
  return #names
end

function selfName(context)
  return context:node().name
end

function parentName(context)
  local parent = context:node().parent
  return if parent then parent.name else '<none>'
end

function missingNames(context)
  local artboard = context:artboard()
  return artboard:node('nope') == nil, artboard:node('') == nil
end

function bounds(context)
  local min, max = context:artboard():bounds()
  return min.x, min.y, max.x, max.y
end

function keep(context)
  kept = context:artboard():node('muzzle')
  keptView = context:artboard()
end

function keptReads()
  return kept.x,
    kept.scaleX,
    kept.name,
    #kept.children,
    kept.parent == nil,
    keptView.width,
    keptView:node('muzzle') == nil,
    #keptView.children
end

function keptWrites()
  kept.x = 5
  kept.position = Vector.xy(1, 2)
  kept:decompose(Mat2D.identity())
end

function keptBadKey()
  return kept.bogus
end

function bothNil(context)
  return context:artboard() == nil, context:node() == nil
end

function artboardOnly(context)
  return context:artboard() ~= nil, context:node() == nil
end
)";

// Calls global `name` with the value at stack slot `context` (absolute) and
// `args` numbers, leaving `results` values on the stack.
bool call(
    lua_State* L,
    const char* name,
    int context,
    int results,
    std::initializer_list<double> args = {})
{
  lua_getglobal(L, name);
  int count = 0;
  if (context != 0)
  {
    lua_pushvalue(L, context);
    count++;
  }
  for (double arg : args)
  {
    lua_pushnumber(L, arg);
    count++;
  }
  return lua_pcall(L, count, results, 0) == LUA_OK;
}

size_t topLevelTransforms(Artboard* artboard)
{
  size_t count = 0;
  for (auto child : artboard->children())
  {
    if (child->is<TransformComponent>())
    {
      count++;
    }
  }
  return count;
}
} // namespace

TEST_CASE(
    "context:artboard walks the artboard the script lives in",
    "[scripting]")
{
  ScriptingTest vm(kScript);
  lua_State* L = vm.state();
  auto file = ReadRiveFile("assets/joel_v3.riv", vm.serializer());
  auto instance = file->artboardNamed("Character");
  REQUIRE(instance != nullptr);
  auto weapon = instance->find<TransformComponent>("Weapon");
  auto muzzle = instance->find<TransformComponent>("muzzle");
  REQUIRE(weapon != nullptr);
  REQUIRE(muzzle != nullptr);

  ComponentScriptedObject object(weapon);
  lua_newrive<ScriptedContext>(L, &object);
  int context = lua_gettop(L);

  REQUIRE(call(L, "muzzleX", context, 1));
  CHECK(lua_tonumber(L, -1) == muzzle->x());
  lua_pop(L, 1);

  REQUIRE(call(L, "setMuzzleX", context, 0, {50}));
  CHECK(muzzle->x() == 50.0f);

  REQUIRE(call(L, "sameWrappers", context, 2));
  CHECK(lua_toboolean(L, -2));
  CHECK(lua_toboolean(L, -1));
  lua_pop(L, 2);

  REQUIRE(call(L, "viewSize", context, 2));
  CHECK(lua_tonumber(L, -2) == instance->width());
  CHECK(lua_tonumber(L, -1) == instance->height());
  lua_pop(L, 2);

  REQUIRE(call(L, "topLevel", context, 1));
  CHECK(lua_tonumber(L, -1) == topLevelTransforms(instance.get()));
  lua_pop(L, 1);

  REQUIRE(call(L, "selfName", context, 1));
  CHECK(std::string(lua_tostring(L, -1)) == "Weapon");
  lua_pop(L, 1);

  REQUIRE(call(L, "parentName", context, 1));
  auto parent = weapon->parent();
  REQUIRE(parent != nullptr);
  CHECK(std::string(lua_tostring(L, -1)) == parent->name());
  lua_pop(L, 1);

  REQUIRE(call(L, "missingNames", context, 2));
  CHECK(lua_toboolean(L, -2));
  CHECK(lua_toboolean(L, -1));
  lua_pop(L, 2);

  REQUIRE(call(L, "bounds", context, 4));
  auto aabb = instance->bounds();
  CHECK(lua_tonumber(L, -4) == aabb.minX);
  CHECK(lua_tonumber(L, -3) == aabb.minY);
  CHECK(lua_tonumber(L, -2) == aabb.maxX);
  CHECK(lua_tonumber(L, -1) == aabb.maxY);
  lua_pop(L, 4);

  lua_pop(L, 1); // context
}

TEST_CASE(
    "nodes kept from context:artboard go quiet once the object is gone",
    "[scripting]")
{
  ScriptingTest vm(kScript);
  lua_State* L = vm.state();
  auto file = ReadRiveFile("assets/joel_v3.riv", vm.serializer());
  auto instance = file->artboardNamed("Character");
  REQUIRE(instance != nullptr);
  auto weapon = instance->find<TransformComponent>("Weapon");
  REQUIRE(weapon != nullptr);

  ComponentScriptedObject object(weapon);
  auto scriptedContext = lua_newrive<ScriptedContext>(L, &object);
  int context = lua_gettop(L);
  REQUIRE(call(L, "keep", context, 0));

  // The object goes away first, then its artboard; the kept node and view
  // must not reach into the freed artboard.
  scriptedContext->clearScriptedObject();
  instance.reset();

  REQUIRE(call(L, "keptReads", 0, 8));
  CHECK(lua_tonumber(L, -8) == 0.0);
  CHECK(lua_tonumber(L, -7) == 1.0);
  CHECK(std::string(lua_tostring(L, -6)).empty());
  CHECK(lua_tonumber(L, -5) == 0.0);
  CHECK(lua_toboolean(L, -4));
  CHECK(lua_tonumber(L, -3) == 0.0);
  CHECK(lua_toboolean(L, -2));
  CHECK(lua_tonumber(L, -1) == 0.0);
  lua_pop(L, 8);

  CHECK(call(L, "keptWrites", 0, 0));
  // Writes drop whatever the key, as a NodeRef's do; a read of a key a node
  // does not have still raises.
  REQUIRE_FALSE(call(L, "keptBadKey", 0, 0));
  CHECK(
      std::string(lua_tostring(L, -1)).find("not a valid index") !=
      std::string::npos);
  lua_pop(L, 1); // error message

  lua_pop(L, 1); // context
}

TEST_CASE(
    "context:artboard and context:node are nil without a component",
    "[scripting]")
{
  ScriptingTest vm(kScript);
  lua_State* L = vm.state();
  ScriptedObjectTest object;
  lua_newrive<ScriptedContext>(L, &object);
  int context = lua_gettop(L);
  REQUIRE(call(L, "bothNil", context, 2));
  CHECK(lua_toboolean(L, -2));
  CHECK(lua_toboolean(L, -1));
  lua_pop(L, 3);
}

TEST_CASE(
    "context:node is nil for a component that is not a transform",
    "[scripting]")
{
  ScriptingTest vm(kScript);
  lua_State* L = vm.state();
  auto file = ReadRiveFile("assets/joel_v3.riv", vm.serializer());
  auto instance = file->artboardNamed("Character");
  REQUIRE(instance != nullptr);
  Component* notTransform = nullptr;
  for (auto object : instance->objects())
  {
    if (object != nullptr && object->is<Component>() &&
        !object->is<TransformComponent>())
    {
      notTransform = object->as<Component>();
      break;
    }
  }
  REQUIRE(notTransform != nullptr);

  ComponentScriptedObject object(notTransform);
  lua_newrive<ScriptedContext>(L, &object);
  int context = lua_gettop(L);
  REQUIRE(call(L, "artboardOnly", context, 2));
  CHECK(lua_toboolean(L, -2));
  CHECK(lua_toboolean(L, -1));
  lua_pop(L, 3);
}

TEST_CASE("a read goes stale with the dirt it asked for", "[scripting]")
{
  ScriptingTest vm(kScript);
  auto file = ReadRiveFile("assets/joel_v3.riv", vm.serializer());
  auto instance = file->artboardNamed("Character");
  REQUIRE(instance != nullptr);
  auto weapon = instance->find<TransformComponent>("Weapon");
  auto muzzle = instance->find<TransformComponent>("muzzle");
  REQUIRE(weapon != nullptr);
  REQUIRE(muzzle != nullptr);

  ComponentScriptedObject object(weapon);
  ScriptedReadTracker tracker(instance->objects().size());
  // Read in update 10: muzzle's world transform, twice, and its transform.
  tracker.track(
      &object,
      ScriptedReadPhase::update,
      10,
      {{muzzle, ComponentDirt::WorldTransform},
       {muzzle, ComponentDirt::Transform},
       {muzzle, ComponentDirt::WorldTransform}});
  REQUIRE(tracker.isRead(muzzle->graphOrder()));
  CHECK_FALSE(tracker.isRead(weapon->graphOrder()));

  // Dirt it did not ask for, and an update it already saw, leave it be.
  tracker.beginUpdates();
  tracker.updated(muzzle, ComponentDirt::Paint, 11);
  tracker.updated(muzzle, ComponentDirt::WorldTransform, 10);
  CHECK_FALSE(tracker.hasDue());
  CHECK_FALSE(tracker.runDue());
  CHECK(object.dirt == ComponentDirt::None);

  // A later update of what it read makes it due, and running it dirties it
  // to update again.
  tracker.updated(muzzle, ComponentDirt::Transform, 11);
  CHECK(tracker.hasDue());
  CHECK(tracker.runDue());
  CHECK(
      (object.dirt & ComponentDirt::ScriptUpdate) ==
      ComponentDirt::ScriptUpdate);
  CHECK_FALSE(tracker.hasDue());

  // Due again in the same call, as when it writes what it reads: it waits
  // for the next call.
  object.dirt = ComponentDirt::None;
  tracker.updated(muzzle, ComponentDirt::WorldTransform, 12);
  CHECK_FALSE(tracker.runDue());
  CHECK(tracker.hasDue());
  CHECK(object.dirt == ComponentDirt::None);
  tracker.beginUpdates();
  CHECK(tracker.runDue());
  CHECK(object.dirt != ComponentDirt::None);

  // Running again replaces what it read; what it read before no longer
  // counts.
  tracker.track(
      &object,
      ScriptedReadPhase::update,
      20,
      {{weapon, ComponentDirt::Transform}});
  CHECK_FALSE(tracker.isRead(muzzle->graphOrder()));
  CHECK(tracker.isRead(weapon->graphOrder()));
  tracker.beginUpdates();
  tracker.updated(muzzle, ComponentDirt::Transform, 21);
  CHECK_FALSE(tracker.hasDue());

  tracker.untrack(&object);
  CHECK_FALSE(tracker.isRead(weapon->graphOrder()));
  tracker.updated(weapon, ComponentDirt::Transform, 22);
  CHECK_FALSE(tracker.hasDue());
}

TEST_CASE("reads drop with the dependency order", "[scripting]")
{
  ScriptingTest vm(kScript);
  auto file = ReadRiveFile("assets/joel_v3.riv", vm.serializer());
  auto instance = file->artboardNamed("Character");
  REQUIRE(instance != nullptr);
  auto muzzle = instance->find<TransformComponent>("muzzle");
  REQUIRE(muzzle != nullptr);

  ComponentScriptedObject object(muzzle);
  ScriptedReadTracker tracker(instance->objects().size());
  tracker.track(
      &object,
      ScriptedReadPhase::update,
      1,
      {{muzzle, ComponentDirt::Transform}});
  REQUIRE(tracker.isRead(muzzle->graphOrder()));
  // What it read may sit elsewhere in a new order, so it reads afresh.
  tracker.dependencyOrderChanged(instance->objects().size());
  CHECK_FALSE(tracker.isRead(muzzle->graphOrder()));
  CHECK(tracker.hasDue());
  // A second reorder before it runs keeps it due, though its reads are gone.
  tracker.dependencyOrderChanged(instance->objects().size());
  CHECK(tracker.hasDue());
  // An advance that reads nothing meanwhile keeps the update due too.
  CHECK(tracker.track(&object, ScriptedReadPhase::advance, 2, {}));
  CHECK(tracker.hasDue());
  tracker.beginUpdates();
  CHECK(tracker.runDue());
  CHECK(
      (object.dirt & ComponentDirt::ScriptUpdate) ==
      ComponentDirt::ScriptUpdate);
  // Run, reading nothing, it is no longer tracked.
  CHECK_FALSE(tracker.track(&object, ScriptedReadPhase::update, 3, {}));
  CHECK_FALSE(tracker.hasDue());
}

TEST_CASE(
    "an advance read wakes the advance, apart from the update",
    "[scripting]")
{
  ScriptingTest vm(kScript);
  auto file = ReadRiveFile("assets/joel_v3.riv", vm.serializer());
  auto instance = file->artboardNamed("Character");
  REQUIRE(instance != nullptr);
  auto weapon = instance->find<TransformComponent>("Weapon");
  auto muzzle = instance->find<TransformComponent>("muzzle");
  REQUIRE(weapon != nullptr);
  REQUIRE(muzzle != nullptr);

  ComponentScriptedObject object(weapon);
  ScriptedReadTracker tracker(instance->objects().size());
  // Its update read Weapon and its advance read muzzle; each set replaces
  // only its own.
  CHECK(tracker.track(
      &object,
      ScriptedReadPhase::update,
      5,
      {{weapon, ComponentDirt::Transform}}));
  CHECK(tracker.track(
      &object,
      ScriptedReadPhase::advance,
      5,
      {{muzzle, ComponentDirt::WorldTransform}}));
  CHECK(tracker.isRead(weapon->graphOrder()));
  CHECK(tracker.isRead(muzzle->graphOrder()));

  // muzzle moving wakes the advance and leaves the update be.
  tracker.beginUpdates();
  tracker.updated(muzzle, ComponentDirt::WorldTransform, 6);
  CHECK(tracker.hasDue());
  CHECK(tracker.runDue());
  CHECK(object.advanceWakes == 1);
  CHECK(object.dirt == ComponentDirt::None);
  CHECK_FALSE(tracker.hasDue());

  // An advance wakes each time, as each wake runs a later frame's advance.
  tracker.updated(muzzle, ComponentDirt::WorldTransform, 7);
  CHECK(tracker.runDue());
  CHECK(object.advanceWakes == 2);

  // Weapon moving runs the update and does not wake the advance.
  tracker.updated(weapon, ComponentDirt::Transform, 8);
  CHECK(tracker.runDue());
  CHECK(object.advanceWakes == 2);
  CHECK(
      (object.dirt & ComponentDirt::ScriptUpdate) ==
      ComponentDirt::ScriptUpdate);

  // An advance that reads nothing drops its set; the update's still counts.
  CHECK(tracker.track(&object, ScriptedReadPhase::advance, 9, {}));
  CHECK_FALSE(tracker.isRead(muzzle->graphOrder()));
  CHECK(tracker.isRead(weapon->graphOrder()));
  // With neither set reading anything, nothing is tracked.
  CHECK_FALSE(tracker.track(&object, ScriptedReadPhase::update, 9, {}));
  CHECK_FALSE(tracker.isRead(weapon->graphOrder()));
}

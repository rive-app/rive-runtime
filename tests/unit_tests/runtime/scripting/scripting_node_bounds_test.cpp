#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/artboard.hpp"
#include "rive/transform_component.hpp"
#include "rive/shapes/points_path.hpp"
#include "rive/shapes/polygon.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/bones/root_bone.hpp"
#include "rive/bones/skin.hpp"
#include "rive_file_reader.hpp"
#include <cmath>
#include <memory>
#include <string>

// NodeData:bounds() and NodeData:worldBounds() on node_bounds.riv (RML in
// assets/rml/node_bounds), which has one node of each kind that has a box of
// its own, and a group that has none.

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
    return true;
  }
  uint32_t assetId() override { return 0; }
  ScriptProtocol scriptProtocol() override { return ScriptProtocol::utility; }
  void markNeedsUpdate() override {}
  Component* component() override { return m_component; }

private:
  Component* m_component;
};

const char* kScript = R"(
local kept = nil

local function unpackBounds(node)
  local min, max = node:bounds()
  local wmin, wmax = node:worldBounds()
  return min.x, min.y, max.x, max.y, wmin.x, wmin.y, wmax.x, wmax.y
end

function nodeBounds(context, name)
  return unpackBounds(context:artboard():node(name))
end

function keep(context)
  kept = context:artboard():node('Panel')
end

function keptBounds()
  return unpackBounds(kept)
end
)";

struct Bounds
{
  AABB local;
  AABB world;
};

// Calls global `name` with the context at stack slot `context` (absolute)
// and `arg`, if given, reading back the eight numbers it returns.
Bounds callBounds(
    lua_State* L,
    const char* name,
    int context,
    const char* arg = nullptr)
{
  lua_getglobal(L, name);
  int count = 0;
  if (context != 0)
  {
    lua_pushvalue(L, context);
    count++;
  }
  if (arg != nullptr)
  {
    lua_pushstring(L, arg);
    count++;
  }
  int status = lua_pcall(L, count, 8, 0);
  if (status != LUA_OK)
  {
    FAIL(lua_tostring(L, -1));
  }
  Bounds out;
  out.local = AABB(
      (float)lua_tonumber(L, -8),
      (float)lua_tonumber(L, -7),
      (float)lua_tonumber(L, -6),
      (float)lua_tonumber(L, -5));
  out.world = AABB(
      (float)lua_tonumber(L, -4),
      (float)lua_tonumber(L, -3),
      (float)lua_tonumber(L, -2),
      (float)lua_tonumber(L, -1));
  lua_pop(L, 8);
  return out;
}

bool sameBox(const AABB& a, const AABB& b)
{
  return a.minX == b.minX && a.minY == b.minY && a.maxX == b.maxX &&
         a.maxY == b.maxY;
}

// Within float noise: a handle placed by its angle lands a hair off.
bool nearBox(const AABB& a, const AABB& b)
{
  const float tolerance = 1e-3f;
  return std::abs(a.minX - b.minX) < tolerance &&
         std::abs(a.minY - b.minY) < tolerance &&
         std::abs(a.maxX - b.maxX) < tolerance &&
         std::abs(a.maxY - b.maxY) < tolerance;
}

std::string describe(const AABB& box)
{
  return std::to_string(box.minX) + "," + std::to_string(box.minY) + " " +
         std::to_string(box.maxX) + "," + std::to_string(box.maxY);
}
} // namespace

TEST_CASE("NodeData reads the box of each kind of node", "[scripting]")
{
  ScriptingTest vm(kScript);
  lua_State* L = vm.state();
  auto file = ReadRiveFile("assets/node_bounds.riv", vm.serializer());
  auto instance = file->artboardNamed("Main");
  REQUIRE(instance != nullptr);
  // Lay out and pose everything.
  instance->advance(0.0f);
  instance->advance(0.0f);

  auto group = instance->find<TransformComponent>("Group");
  REQUIRE(group != nullptr);
  ComponentScriptedObject object(group);
  lua_newrive<ScriptedContext>(L, &object);
  int context = lua_gettop(L);

  auto check = [&](const char* name, AABB local, AABB world) {
    INFO(name);
    auto component = instance->find<TransformComponent>(name);
    REQUIRE(component != nullptr);
    Bounds read = callBounds(L, "nodeBounds", context, name);
    INFO(
        "local " << describe(read.local) << ", world " << describe(read.world));
    // What the runtime knows the node's box to be...
    CHECK(sameBox(read.local, component->localBounds()));
    CHECK(sameBox(
        read.world,
        component->worldTransform().mapBoundingBox(component->localBounds())));
    // ...and where the markup put it.
    CHECK(nearBox(read.local, local));
    CHECK(nearBox(read.world, world));
  };

  // A layout's box starts at its origin, its top left.
  check("Panel", AABB(0, 0, 200, 100), AABB(0, 0, 200, 100));
  // Nested artboards measure the artboard they show, with or without a
  // layout placing them.
  check("Button", AABB(0, 0, 80, 24), AABB(10, 8, 90, 32));
  check("Badge", AABB(0, 0, 80, 24), AABB(250, 200, 330, 224));
  // A shape's box holds its paths, the rectangle drawn about its center.
  check("Box", AABB(-20, -15, 20, 15), AABB(280, 35, 320, 65));
  check("Rect", AABB(-20, -15, 20, 15), AABB(280, 35, 320, 65));
  check("Label", AABB(0, 0, 120, 30), AABB(20, 250, 140, 280));
  // A group has no size of its own: a zero box, at its origin in the world.
  check("Group", AABB(), AABB(10, 200, 10, 200));
  check("Child", AABB(), AABB(15, 205, 15, 205));
  // A shape's box holds its paths' control points, here a handle at 100, 90
  // reaching past the curve's peak at 40.
  check("Curve", AABB(0, 0, 100, 90), AABB(100, 150, 200, 240));
  // Placed by the node holding it, scaled 2 wide. Pair's path with no points
  // adds nothing to its box, not even Pair's origin.
  check("Pair", AABB(15, 5, 35, 15), AABB(165, 255, 185, 265));
  check("Offset", AABB(-5, -5, 5, 5), AABB(165, 255, 185, 265));

  // A shape with no paths, and a path with no points, measure as the
  // inverted box an expansion starts from; a script reads it as no size,
  // like a group.
  for (auto [name, origin] :
       {std::make_pair("Empty", Vec2D(50, 50)),
        std::make_pair("Blank", Vec2D(150, 250))})
  {
    INFO(name);
    auto empty = instance->find<TransformComponent>(name);
    REQUIRE(empty != nullptr);
    CHECK(empty->localBounds().minX > empty->localBounds().maxX);
    Bounds read = callBounds(L, "nodeBounds", context, name);
    INFO(
        "local " << describe(read.local) << ", world " << describe(read.world));
    CHECK(sameBox(read.local, AABB()));
    CHECK(sameBox(read.world, AABB(origin.x, origin.y, origin.x, origin.y)));
  }

  {
    // A path's own box is its curve's, not its control points'.
    auto arc = instance->find<TransformComponent>("Arc");
    REQUIRE(arc != nullptr);
    Bounds read = callBounds(L, "nodeBounds", context, "Arc");
    INFO(
        "local " << describe(read.local) << ", world " << describe(read.world));
    CHECK(sameBox(read.local, arc->localBounds()));
    CHECK(read.local.maxY < 90.0f);
  }

  lua_pop(L, 1); // context
}

TEST_CASE(
    "NodeData bounds read as a zero box once the object is gone",
    "[scripting]")
{
  ScriptingTest vm(kScript);
  lua_State* L = vm.state();
  auto file = ReadRiveFile("assets/node_bounds.riv", vm.serializer());
  auto instance = file->artboardNamed("Main");
  REQUIRE(instance != nullptr);
  instance->advance(0.0f);
  auto group = instance->find<TransformComponent>("Group");
  REQUIRE(group != nullptr);

  ComponentScriptedObject object(group);
  auto scriptedContext = lua_newrive<ScriptedContext>(L, &object);
  int context = lua_gettop(L);
  lua_getglobal(L, "keep");
  lua_pushvalue(L, context);
  REQUIRE(lua_pcall(L, 1, 0, 0) == LUA_OK);
  // Alive, the kept panel reads its box.
  CHECK(sameBox(callBounds(L, "keptBounds", 0).local, AABB(0, 0, 200, 100)));

  scriptedContext->clearScriptedObject();
  instance.reset();
  Bounds read = callBounds(L, "keptBounds", 0);
  CHECK(sameBox(read.local, AABB()));
  CHECK(sameBox(read.world, AABB()));

  lua_pop(L, 1); // context
}

TEST_CASE(
    "NodeData bounds read a hidden shape's current geometry",
    "[scripting]")
{
  ScriptingTest vm(kScript);
  lua_State* L = vm.state();
  auto file = ReadRiveFile("assets/node_bounds.riv", vm.serializer());
  auto instance = file->artboardNamed("Main");
  REQUIRE(instance != nullptr);
  auto box = instance->find<Shape>("Box");
  auto rect = instance->find<Rectangle>("Rect");
  REQUIRE(box != nullptr);
  REQUIRE(rect != nullptr);
  // Invisible, the shape defers building its paths.
  box->opacity(0.0f);
  instance->advance(0.0f);
  instance->advance(0.0f);

  ComponentScriptedObject object(box);
  lua_newrive<ScriptedContext>(L, &object);
  int context = lua_gettop(L);
  auto check = [&](float halfWidth) {
    for (const char* name : {"Box", "Rect"})
    {
      INFO(name << " half width " << halfWidth);
      Bounds read = callBounds(L, "nodeBounds", context, name);
      INFO(
          "local " << describe(read.local) << ", world "
                   << describe(read.world));
      CHECK(nearBox(read.local, AABB(-halfWidth, -15, halfWidth, 15)));
      CHECK(
          nearBox(read.world, AABB(300 - halfWidth, 35, 300 + halfWidth, 65)));
    }
  };
  rect->width(60.0f);
  // Read before the rectangle places its points, it measures as it last
  // drew. That box must not outlive the update that places them.
  check(20.0f);
  instance->advance(0.0f);
  REQUIRE(rect->needsPathBuild());
  check(30.0f);
  lua_pop(L, 1); // context
}

TEST_CASE("NodeData bounds read a skinned path in its own space", "[scripting]")
{
  ScriptingTest vm(kScript);
  lua_State* L = vm.state();
  auto file = ReadRiveFile("assets/node_bounds.riv", vm.serializer());
  auto instance = file->artboardNamed("Main");
  REQUIRE(instance != nullptr);
  instance->advance(0.0f);
  instance->advance(0.0f);
  auto skinned = instance->find<PointsPath>("Skinned");
  auto bone = instance->find<RootBone>("Bone");
  REQUIRE(skinned != nullptr);
  REQUIRE(skinned->skin() != nullptr);
  REQUIRE(bone != nullptr);

  ComponentScriptedObject object(skinned);
  lua_newrive<ScriptedContext>(L, &object);
  int context = lua_gettop(L);
  auto check = [&](const char* name, AABB local, AABB world) {
    INFO(name);
    Bounds read = callBounds(L, "nodeBounds", context, name);
    INFO(
        "local " << describe(read.local) << ", world " << describe(read.world));
    CHECK(nearBox(read.local, local));
    CHECK(nearBox(read.world, world));
  };
  // Bound where it is, the skin leaves it as authored. Its points are in
  // the artboard's space, which bounds() takes back to its own.
  check("Skinned", AABB(0, 0, 40, 20), AABB(200, 100, 240, 120));
  check("Limb", AABB(0, 0, 40, 20), AABB(200, 100, 240, 120));
  // The bone moves its points, not the path or its shape.
  bone->x(250.0f);
  instance->advance(0.0f);
  check("Skinned", AABB(50, 0, 90, 20), AABB(250, 100, 290, 120));
  check("Limb", AABB(50, 0, 90, 20), AABB(250, 100, 290, 120));
  lua_pop(L, 1); // context
}

TEST_CASE(
    "NodeData bounds read a hidden polygon's geometry, not its frame",
    "[scripting]")
{
  ScriptingTest vm(kScript);
  lua_State* L = vm.state();
  auto file = ReadRiveFile("assets/node_bounds.riv", vm.serializer());
  auto instance = file->artboardNamed("Main");
  REQUIRE(instance != nullptr);
  instance->advance(0.0f);
  instance->advance(0.0f);
  auto tri = instance->find<Shape>("Tri");
  auto poly = instance->find<Polygon>("Poly");
  REQUIRE(tri != nullptr);
  REQUIRE(poly != nullptr);

  ComponentScriptedObject object(tri);
  lua_newrive<ScriptedContext>(L, &object);
  int context = lua_gettop(L);
  // A triangle's points sit at the top of its frame and on a line three
  // quarters of the way down: its box is narrower and shorter than the
  // frame.
  auto check = [&](float halfWidth) {
    for (const char* name : {"Tri", "Poly"})
    {
      INFO(name << " half width " << halfWidth);
      Bounds read = callBounds(L, "nodeBounds", context, name);
      INFO(
          "local " << describe(read.local) << ", world "
                   << describe(read.world));
      CHECK(nearBox(read.local, AABB(-halfWidth, -50, halfWidth, 25)));
      CHECK(nearBox(
          read.world,
          AABB(300 - halfWidth, 150, 300 + halfWidth, 225)));
    }
  };
  check(43.30127f);
  // Hidden, it widens without building its path.
  tri->opacity(0.0f);
  instance->advance(0.0f);
  poly->width(120.0f);
  instance->advance(0.0f);
  REQUIRE(poly->needsPathBuild());
  check(51.961524f);
  // Shown again, it builds the same path.
  tri->opacity(1.0f);
  instance->advance(0.0f);
  REQUIRE_FALSE(poly->needsPathBuild());
  check(51.961524f);
  lua_pop(L, 1); // context
}

TEST_CASE("NodeData bounds of a shape scaled to zero on an axis", "[scripting]")
{
  ScriptingTest vm(kScript);
  lua_State* L = vm.state();
  auto file = ReadRiveFile("assets/node_bounds.riv", vm.serializer());
  auto instance = file->artboardNamed("Main");
  REQUIRE(instance != nullptr);
  auto box = instance->find<Shape>("Box");
  auto pair = instance->find<Shape>("Pair");
  REQUIRE(box != nullptr);
  REQUIRE(pair != nullptr);
  // With no inverse world transform, a shape's box still comes from its
  // paths' transforms in its space, through the node holding Offset too.
  box->scaleX(0.0f);
  pair->scaleY(0.0f);
  instance->advance(0.0f);
  instance->advance(0.0f);

  ComponentScriptedObject object(box);
  lua_newrive<ScriptedContext>(L, &object);
  int context = lua_gettop(L);
  auto check = [&](const char* name, AABB local, AABB world) {
    INFO(name);
    Bounds read = callBounds(L, "nodeBounds", context, name);
    INFO(
        "local " << describe(read.local) << ", world " << describe(read.world));
    CHECK(nearBox(read.local, local));
    CHECK(nearBox(read.world, world));
  };
  check("Box", AABB(-20, -15, 20, 15), AABB(300, 35, 300, 65));
  check("Rect", AABB(-20, -15, 20, 15), AABB(300, 35, 300, 65));
  check("Pair", AABB(15, 5, 35, 15), AABB(165, 250, 185, 250));
  check("Offset", AABB(-5, -5, 5, 5), AABB(165, 250, 185, 250));
  lua_pop(L, 1); // context
}

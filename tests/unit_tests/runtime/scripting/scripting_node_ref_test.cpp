#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/file.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/node.hpp"
#include "rive/scripted/scripted_object.hpp"
#include "rive/generated/component_base.hpp"
#include "rive/generated/node_base.hpp"
#include "rive/generated/shapes/shape_base.hpp"
#include "rive/generated/shapes/rectangle_base.hpp"
#include "rive/generated/script_input_component_base.hpp"
#include "rive/generated/scripted/scripted_drawable_base.hpp"
#include "riv_bytes.hpp"
#include "rive_file_reader.hpp"

using namespace rive;

// A Luau NodeRef's nodes live as long as the scripted object: a node the
// script kept reads as an empty node once it is gone, never its freed
// component, and drops writes; while it lives they land. get() hands out
// one node, made once.
TEST_CASE("a Luau NodeRef's kept node outlives nothing", "[scripting]")
{
  RivBytes riv;
  writeArtboard(riv);                   // 0
  riv.object(rive::ShapeBase::typeKey); // 1
  riv.propUint(rive::ComponentBase::parentIdPropertyKey, 0);
  riv.propFloat(rive::NodeBase::xPropertyKey, 30.0f);
  riv.end();
  riv.object(rive::RectangleBase::typeKey); // 2
  riv.propUint(rive::ComponentBase::parentIdPropertyKey, 1);
  riv.end();
  riv.object(rive::ScriptedDrawableBase::typeKey); // 3
  riv.propUint(rive::ComponentBase::parentIdPropertyKey, 0);
  riv.end();
  auto bytes = riv.bytes();

  ScriptingTest vm("local kept = nil\n"
                   "function keep(ref)\n"
                   "  kept = ref:get()\n"
                   "  return kept.x\n"
                   "end\n"
                   "function children()\n"
                   "  return #kept.children\n"
                   "end\n"
                   "function write()\n"
                   "  kept.x = 5\n"
                   "  return kept.x\n"
                   "end\n"
                   "driver = {\n"
                   "  advance = function(self, seconds)\n"
                   "    kept.x = 45\n"
                   "    return false\n"
                   "  end,\n"
                   "}\n"
                   "function same(ref)\n"
                   "  return ref:get() == ref:get() and ref:get() == kept\n"
                   "end\n"
                   "function readKept()\n"
                   "  return kept.x\n"
                   "end\n"
                   "function paint()\n"
                   "  return kept.paint == nil and kept:asPaint() == nil\n"
                   "end\n"
                   "function fresh(ref)\n"
                   "  return ref:get() == nil\n"
                   "end\n");
  lua_State* L = vm.state();
  ImportResult result;
  auto file = File::import(bytes, vm.serializer(), &result);
  REQUIRE(result == ImportResult::success);
  auto artboard = file->artboard();
  auto object = ScriptedObject::from(artboard->objects()[3]);
  REQUIRE(object != nullptr);

  auto lease = make_rcp<ScriptedObjectLease>(object);
  lua_newrive<ScriptedNodeRef>(L, L, lease, 2u); // the shape, index 1

  auto call = [&](const char* name, bool withRef) {
    lua_getglobal(L, name);
    int args = 0;
    if (withRef)
    {
      lua_pushvalue(L, -2);
      args = 1;
    }
    REQUIRE(lua_pcall(L, args, 1, 0) == LUA_OK);
    double value = lua_isboolean(L, -1) ? (lua_toboolean(L, -1) ? 1 : 0)
                                        : lua_tonumber(L, -1);
    lua_pop(L, 1);
    return value;
  };

  CHECK(call("keep", true) == 30);
  CHECK(call("children", false) == 1);
  CHECK(call("same", true) == 1);
  // A write lands, outside advance and in it.
  CHECK(call("write", false) == 5);
  CHECK(artboard->objects()[1]->as<Node>()->x() == 5.0f);
  lua_getglobal(L, "driver");
  int driver = lua_ref(L, -1);
  lua_pop(L, 1);
  vm.vm()->callAdvance(object, driver, 0.0f);
  lua_unref(L, driver);
  CHECK(artboard->objects()[1]->as<Node>()->x() == 45.0f);
  CHECK(call("readKept", false) == 45);

  CHECK(call("fresh", true) == 0);

  // The scripted object goes away: the node the script kept reads as an
  // empty node and drops writes, and the NodeRef names nothing.
  lease->end();
  CHECK(call("readKept", false) == 0);
  CHECK(call("write", false) == 0);
  CHECK(artboard->objects()[1]->as<Node>()->x() == 45.0f);
  CHECK(call("children", false) == 0);
  CHECK(call("fresh", true) == 1);

  // Nor does anything read through the freed artboard.
  artboard = nullptr;
  file = nullptr;
  CHECK(call("readKept", false) == 0);
  CHECK(call("paint", false) == 1);
  CHECK(call("children", false) == 0);
}

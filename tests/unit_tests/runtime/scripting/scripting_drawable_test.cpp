/*
 * Copyright 2025 Rive
 */

#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/scripted/scripted_drawable.hpp"
#include "utils/no_op_renderer.hpp"
#include "rive/math/mat2d.hpp"

using namespace rive;

TEST_CASE("scripted drawable draw function can be called", "[scripting]")
{
    ScriptingTest vm(
        R"(type MyDrawing = {}
local drawCallCount = 0

function init(self: MyDrawing, context: Context): boolean
  return true
end

function draw(self: MyDrawing, renderer: Renderer)
  drawCallCount = drawCallCount + 1
  local path: Path = Path.new()
  local paint: Paint = Paint.with({color=0xFFFF0000})
  renderer:drawPath(path, paint)
end

function getDrawCallCount(): number
  return drawCallCount
end

return function(): Node<MyNode>
  return {
    init = init,
    draw = draw,
  }
end
)");
    lua_State* L = vm.state();
    auto top = lua_gettop(L);
    // ScriptingTest executes the factory function and leaves the result table
    // on the stack

    // Test that the draw function exists and can be called
    {
        lua_getglobal(L, "draw");
        lua_pushvalue(L, -2); // Push self (the table from factory function)

        // Create a renderer
        NoOpRenderer renderer;
        auto scriptedRenderer = lua_newrive<ScriptedRenderer>(L, &renderer);

        CHECK(lua_pcall(L, 2, 0, 0) == LUA_OK);
        CHECK(scriptedRenderer->end());
        CHECK(top == lua_gettop(L));
    }

    // Verify draw was called
    {
        lua_getglobal(L, "getDrawCallCount");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(lua_tonumber(L, -1) == 1.0);
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("scripted drawable advance function can be called", "[scripting]")
{
    ScriptingTest vm(
        R"(type MyDrawing = {}
local advanceCallCount = 0
local totalElapsed = 0.0

function init(self: MyDrawing, context: Context): boolean
  return true
end

function advance(self: MyDrawing, seconds: number): boolean
  advanceCallCount = advanceCallCount + 1
  totalElapsed = totalElapsed + seconds
  return true
end

function getAdvanceCallCount(): number
  return advanceCallCount
end

function getTotalElapsed(): number
  return totalElapsed
end

return function(): Node<MyNode>
  return {
    init = init,
    advance = advance,
  }
end
)");
    lua_State* L = vm.state();
    auto top = lua_gettop(L);

    // Test advance function
    {
        lua_getglobal(L, "advance");
        lua_pushvalue(L, -2);     // Push self
        lua_pushnumber(L, 0.016); // ~60fps
        CHECK(lua_pcall(L, 2, 1, 0) == LUA_OK);
        CHECK(lua_toboolean(L, -1) == 1);
        lua_pop(L, 1); // Pop result
        CHECK(top == lua_gettop(L));
    }

    // Verify advance was called
    {
        lua_getglobal(L, "getAdvanceCallCount");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(lua_tonumber(L, -1) == 1.0);
        lua_pop(L, 1);

        lua_getglobal(L, "getTotalElapsed");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(lua_tonumber(L, -1) == Approx(0.016));
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("scripted drawable update function can be called", "[scripting]")
{
    ScriptingTest vm(
        R"(type MyDrawing = {}
local updateCallCount = 0

function init(self: MyDrawing, context: Context): boolean
  return true
end

function update(self: MyDrawing)
  updateCallCount = updateCallCount + 1
end

function getUpdateCallCount(): number
  return updateCallCount
end

return function(): Node<MyNode>
  return {
    init = init,
    update = update,
  }
end
)");
    lua_State* L = vm.state();
    auto top = lua_gettop(L);

    // Test update function
    {
        lua_getglobal(L, "update");
        lua_pushvalue(L, -2); // Push self
        CHECK(lua_pcall(L, 1, 0, 0) == LUA_OK);
        CHECK(top == lua_gettop(L));
    }

    // Verify update was called
    {
        lua_getglobal(L, "getUpdateCallCount");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(lua_tonumber(L, -1) == 1.0);
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("scripted drawable pointer events can be handled", "[scripting]")
{
    ScriptingTest vm(
        R"(type MyDrawing = {}
local pointerDownCount = 0
local pointerUpCount = 0
local pointerMoveCount = 0
local pointerExitCount = 0

function init(self: MyDrawing, context: Context): boolean
  return true
end

function pointerDown(self: MyDrawing, event: PointerEvent)
  pointerDownCount = pointerDownCount + 1
end

function pointerUp(self: MyDrawing, event: PointerEvent)
  pointerUpCount = pointerUpCount + 1
end

function pointerMove(self: MyDrawing, event: PointerEvent)
  pointerMoveCount = pointerMoveCount + 1
end

function pointerExit(self: MyDrawing, event: PointerEvent)
  pointerExitCount = pointerExitCount + 1
end

function getPointerDownCount(): number
  return pointerDownCount
end

function getPointerUpCount(): number
  return pointerUpCount
end

function getPointerMoveCount(): number
  return pointerMoveCount
end

function getPointerExitCount(): number
  return pointerExitCount
end

return function(): Node<MyNode>
  return {
    init = init,
    pointerDown = pointerDown,
    pointerUp = pointerUp,
    pointerMove = pointerMove,
    pointerExit = pointerExit,
  }
end
)");
    lua_State* L = vm.state();
    auto top = lua_gettop(L);

    // Test pointerDown
    {
        lua_getglobal(L, "pointerDown");
        lua_pushvalue(L, -2); // Push self
        auto pointerEvent =
            lua_newrive<ScriptedPointerEvent>(L, 1, Vec2D(10.0f, 20.0f));
        CHECK(pointerEvent != nullptr);
        CHECK(lua_pcall(L, 2, 0, 0) == LUA_OK);
        CHECK(top == lua_gettop(L));
    }

    // Test pointerUp
    {
        lua_getglobal(L, "pointerUp");
        lua_pushvalue(L, -2); // Push self
        auto pointerEvent =
            lua_newrive<ScriptedPointerEvent>(L, 1, Vec2D(15.0f, 25.0f));
        CHECK(pointerEvent != nullptr);
        CHECK(lua_pcall(L, 2, 0, 0) == LUA_OK);
        CHECK(top == lua_gettop(L));
    }

    // Test pointerMove
    {
        lua_getglobal(L, "pointerMove");
        lua_pushvalue(L, -2); // Push self
        auto pointerEvent =
            lua_newrive<ScriptedPointerEvent>(L, 1, Vec2D(20.0f, 30.0f));
        CHECK(pointerEvent != nullptr);
        CHECK(lua_pcall(L, 2, 0, 0) == LUA_OK);
        CHECK(top == lua_gettop(L));
    }

    // Test pointerExit
    {
        lua_getglobal(L, "pointerExit");
        lua_pushvalue(L, -2); // Push self
        auto pointerEvent =
            lua_newrive<ScriptedPointerEvent>(L, 1, Vec2D(0.0f, 0.0f));
        CHECK(pointerEvent != nullptr);
        CHECK(lua_pcall(L, 2, 0, 0) == LUA_OK);
        CHECK(top == lua_gettop(L));
    }

    // Verify all events were called
    {
        lua_getglobal(L, "getPointerDownCount");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(lua_tonumber(L, -1) == 1.0);
        lua_pop(L, 1);

        lua_getglobal(L, "getPointerUpCount");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(lua_tonumber(L, -1) == 1.0);
        lua_pop(L, 1);

        lua_getglobal(L, "getPointerMoveCount");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(lua_tonumber(L, -1) == 1.0);
        lua_pop(L, 1);

        lua_getglobal(L, "getPointerExitCount");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(lua_tonumber(L, -1) == 1.0);
        lua_pop(L, 1);

        CHECK(top == lua_gettop(L));
    }
}

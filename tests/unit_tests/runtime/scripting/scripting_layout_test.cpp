/*
 * Copyright 2025 Rive
 */

#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/scripted/scripted_layout.hpp"
#include "rive/layout/layout_enums.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive_file_reader.hpp"
#include "utils/no_op_renderer.hpp"

using namespace rive;

TEST_CASE("scripted layout measure function can be called", "[scripting]")
{
    ScriptingTest vm(
        R"(type MyLayout = {}
local measureCallCount = 0

function init(self: MyLayout, context: Context): boolean
  return true
end

function measure(self: MyLayout): Vector
  measureCallCount = measureCallCount + 1
  return Vector.xy(200, 150)
end

function getMeasureCallCount(): number
  return measureCallCount
end

return function(): Layout<MyLayout>
  return {
    init = init,
    measure = measure,
  }
end
)");
    lua_State* L = vm.state();
    auto top = lua_gettop(L);

    // Test measure function
    {
        lua_getglobal(L, "measure");
        lua_pushvalue(L, -2); // Push self
        CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);

        // Verify return value is a Vec2D
        CHECK(lua_type(L, -1) == LUA_TVECTOR);
        auto size = lua_tovec2d(L, -1);
        CHECK(size->x == 200.0f);
        CHECK(size->y == 150.0f);

        lua_pop(L, 1); // Pop result
        CHECK(top == lua_gettop(L));
    }

    // Verify measure was called
    {
        lua_getglobal(L, "getMeasureCallCount");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(lua_tonumber(L, -1) == 1.0);
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("scripted layout resize function can be called", "[scripting]")
{
    ScriptingTest vm(
        R"(type MyLayout = {}
local resizeCallCount = 0
local lastResizeSize: Vector?

function init(self: MyLayout, context: Context): boolean
  return true
end

function resize(self: MyLayout, size: Vector)
  resizeCallCount = resizeCallCount + 1
  lastResizeSize = size
end

function getResizeCallCount(): number
  return resizeCallCount
end

function getLastResizeSize(): Vector?
  return lastResizeSize
end

return function(): Layout<MyLayout>
  return {
    init = init,
    resize = resize,
  }
end
)");
    lua_State* L = vm.state();
    auto top = lua_gettop(L);

    // Test resize function
    {
        lua_getglobal(L, "resize");
        lua_pushvalue(L, -2); // Push self
        Vec2D testSize(300.0f, 200.0f);
        lua_pushvec2d(L, testSize);
        CHECK(lua_pcall(L, 2, 0, 0) == LUA_OK);
        CHECK(top == lua_gettop(L));
    }

    // Verify resize was called
    {
        lua_getglobal(L, "getResizeCallCount");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(lua_tonumber(L, -1) == 1.0);
        lua_pop(L, 1);

        lua_getglobal(L, "getLastResizeSize");
        CHECK(lua_pcall(L, 0, 1, 0) == LUA_OK);
        CHECK(lua_type(L, -1) == LUA_TVECTOR);
        auto size = lua_tovec2d(L, -1);
        CHECK(size->x == 300.0f);
        CHECK(size->y == 200.0f);
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("scripted layout advance function can be called", "[scripting]")
{
    ScriptingTest vm(
        R"(type MyLayout = {}
local advanceCallCount = 0
local totalElapsed = 0.0

function init(self: MyLayout, context: Context): boolean
  return true
end

function advance(self: MyLayout, seconds: number): boolean
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

return function(): Layout<MyLayout>
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
        lua_pushnumber(L, 0.033); // ~30fps
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
        CHECK(lua_tonumber(L, -1) == Approx(0.033));
        lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
    }
}

TEST_CASE("scripted layout update function can be called", "[scripting]")
{
    ScriptingTest vm(
        R"(type MyLayout = {}
local updateCallCount = 0

function init(self: MyLayout, context: Context): boolean
  return true
end

function update(self: MyLayout)
  updateCallCount = updateCallCount + 1
end

function getUpdateCallCount(): number
  return updateCallCount
end

return function(): Layout<MyLayout>
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

TEST_CASE("scripted layout draw function can be called", "[scripting]")
{
    ScriptingTest vm(
        R"(type MyLayout = {}
local drawCallCount = 0

function init(self: MyLayout, context: Context): boolean
  return true
end

function draw(self: MyLayout, renderer: Renderer)
  drawCallCount = drawCallCount + 1
end

function getDrawCallCount(): number
  return drawCallCount
end

return function(): Layout<MyLayout>
  return {
    init = init,
    draw = draw,
  }
end
)");
    lua_State* L = vm.state();
    auto top = lua_gettop(L);

    // Test draw function
    {
        lua_getglobal(L, "draw");
        lua_pushvalue(L, -2); // Push self

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

TEST_CASE("layout grid script", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/script_layout_test.riv", &silver);
    auto artboard = file->artboardNamed("LayoutScript");

    silver.frameSize(artboard->width(), artboard->height());
    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);
    stateMachine->bindViewModelInstance(vmi);
    auto rows = vmi->propertyValue("Rows")->as<rive::ViewModelInstanceNumber>();
    REQUIRE(rows != nullptr);
    auto rowsValue = rows->propertyValue();
    REQUIRE(rowsValue == 5);
    auto cols =
        vmi->propertyValue("Columns")->as<rive::ViewModelInstanceNumber>();
    REQUIRE(cols != nullptr);
    auto colsValue = cols->propertyValue();
    REQUIRE(colsValue == 5);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = 20;
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    rows->propertyValue(8);
    rowsValue = rows->propertyValue();
    REQUIRE(rowsValue == 8);
    cols->propertyValue(7);
    colsValue = cols->propertyValue();
    REQUIRE(colsValue == 7);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }
    CHECK(silver.matches("script_layout_grid"));
}
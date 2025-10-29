
#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive_file_reader.hpp"

using namespace rive;

TEST_CASE("can access artboard width/height", "[scripting]")
{
    ScriptingTest vm("function accessWidth(artboard:Artboard):()\n"
                     "  return artboard.width\n"
                     "end\n"
                     "function accessHeight(artboard:Artboard):()\n"
                     "  return artboard.height\n"
                     "end\n"
                     "function changeWidth(artboard:Artboard):()\n"
                     "   artboard.width = 24\n"
                     "  return artboard.width\n"
                     "end\n"
                     "function changeHeight(artboard:Artboard):()\n"
                     "  artboard.height = 22\n"
                     "  return artboard.height\n"
                     "end\n");
    lua_State* L = vm.state();
    auto file = ReadRiveFile("assets/coin.riv", vm.serializer());
    auto artboard = file->artboard();
    REQUIRE(artboard != nullptr);
    lua_newrive<ScriptedArtboard>(L, file, artboard->instance());

    lua_getglobal(L, "accessWidth");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
    CHECK(lua_tonumber(L, -1) == 92);
    lua_pop(L, 1);

    lua_getglobal(L, "accessHeight");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
    CHECK(lua_tonumber(L, -1) == 92);
    lua_pop(L, 1);

    lua_getglobal(L, "changeWidth");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
    CHECK(lua_tonumber(L, -1) == 24);
    lua_pop(L, 1);

    lua_getglobal(L, "changeHeight");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
    CHECK(lua_tonumber(L, -1) == 22);
    lua_pop(L, 1);
}

TEST_CASE("can access artboard bounds", "[scripting]")
{
    ScriptingTest vm("function boundsMinX(artboard:Artboard):number\n"
                     "  local min, max = artboard:bounds()\n"
                     "  return min.x\n"
                     "end\n"
                     "function boundsMinY(artboard:Artboard):number\n"
                     "  local min, max = artboard:bounds()\n"
                     "  return min.y\n"
                     "end\n"
                     "function boundsMaxX(artboard:Artboard):number\n"
                     "  local min, max = artboard:bounds()\n"
                     "  return max.x\n"
                     "end\n"
                     "function boundsMaxY(artboard:Artboard):number\n"
                     "  local min, max = artboard:bounds()\n"
                     "  return max.y\n"
                     "end\n");
    lua_State* L = vm.state();
    auto file = ReadRiveFile("assets/coin.riv", vm.serializer());
    auto artboard = file->artboard();
    REQUIRE(artboard != nullptr);
    lua_newrive<ScriptedArtboard>(L, file, artboard->instance());

    lua_getglobal(L, "boundsMinX");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
    CHECK(lua_tonumber(L, -1) == 0);
    lua_pop(L, 1);
    lua_getglobal(L, "boundsMinY");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
    CHECK(lua_tonumber(L, -1) == 0);
    lua_pop(L, 1);
    lua_getglobal(L, "boundsMaxX");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
    CHECK(lua_tonumber(L, -1) == 92);
    lua_pop(L, 1);
    lua_getglobal(L, "boundsMaxY");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
    CHECK(lua_tonumber(L, -1) == 92);
    lua_pop(L, 1);
}

TEST_CASE("can render an artboard via the scripting engine", "[scripting]")
{

    ScriptingTest vm(
        "function render(artboard:Artboard, renderer:Renderer):()\n"
        "  artboard:advance(0.1)\n"
        "  artboard:draw(renderer)\n"
        "  artboard.data.Vertical.value += 5\n"
        "end\n");
    lua_State* L = vm.state();
    auto file = ReadRiveFile("assets/coin.riv", vm.serializer());
    auto artboard = file->artboard();
    REQUIRE(artboard != nullptr);
    lua_newrive<ScriptedArtboard>(L, file, artboard->instance());
    for (int i = 0; i < 10; i++)
    {
        if (i != 0)
        {
            vm.serializer()->addFrame();
        }

        lua_getglobal(L, "render");
        lua_pushvalue(L, -2);
        vm.serializer()->frameSize(artboard->width(), artboard->height());
        auto renderer = vm.serializer()->makeRenderer();
        auto scriptedRenderer =
            lua_newrive<ScriptedRenderer>(L, renderer.get());
        CHECK(lua_pcall(L, 2, 0, 0) == LUA_OK);
        CHECK(scriptedRenderer->end());
    }

    CHECK(vm.serializer()->matches("scripted_artboard_render"));
}

TEST_CASE("can detect hit test from PointerEvent", "[scripting]")
{
    ScriptingTest vm("function pointerDown(event:PointerEvent):number\n"
                     "  event:hit()\n"
                     "  return event.position.x\n"
                     "end\n");

    lua_State* L = vm.state();
    lua_getglobal(L, "pointerDown");
    auto pointerEvent =
        lua_newrive<ScriptedPointerEvent>(L, 1, Vec2D(22.0f, 23.0f));
    CHECK(pointerEvent->m_id == 1);
    CHECK(pointerEvent->m_hitResult == HitResult::none);
    CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
    CHECK(pointerEvent->m_hitResult == HitResult::hitOpaque);
    CHECK(lua_tonumber(L, -1) == 22.0f);
}
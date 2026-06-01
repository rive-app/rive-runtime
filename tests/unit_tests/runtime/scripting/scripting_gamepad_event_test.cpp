#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/input/gamepad_snapshot.hpp"
#include "rive/input/standard_gamepad.hpp"
#include "rive/lua/rive_lua_libs.hpp"

using namespace rive;

TEST_CASE("GamepadEvent reads snapshot fields and standard names",
          "[scripting]")
{
    ScriptingTest vm("function deviceIdOf(g)\n"
                     "  return g.deviceId\n"
                     "end\n"
                     "function mappingOf(g)\n"
                     "  return g.mapping\n"
                     "end\n"
                     "function southPressed(g)\n"
                     "  return g.south\n"
                     "end\n"
                     "function btnPressed(g, i)\n"
                     "  return g:buttonPressed(i)\n"
                     "end\n"
                     "function axisValue(g, i)\n"
                     "  return g:axis(i)\n"
                     "end\n");

    lua_State* L = vm.state();

    GamepadSnapshot snap{};
    snap.deviceId = 42;
    snap.mapping = GamepadMappingKind::standard;
    snap.buttonMask = uint64_t(1)
                      << static_cast<unsigned>(StandardGamepadButton::south);
    snap.buttonValues = {0.5f, 1.f};
    snap.axes = {0.25f, -0.5f, 0.f, 0.f, 0.75f, 0.25f};

    lua_newrive<ScriptedGamepadConnected>(L, snap);

    lua_getglobal(L, "deviceIdOf");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
    CHECK(lua_tointeger(L, -1) == 42);
    lua_pop(L, 1);

    lua_getglobal(L, "mappingOf");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
    CHECK(std::string(lua_tostring(L, -1)) == "standard");
    lua_pop(L, 1);

    lua_getglobal(L, "southPressed");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
    CHECK(lua_toboolean(L, -1));
    lua_pop(L, 1);

    lua_getglobal(L, "btnPressed");
    lua_pushvalue(L, -2);
    lua_pushinteger(L, 1);
    CHECK(lua_pcall(L, 2, 1, 0) == LUA_OK);
    CHECK(lua_toboolean(L, -1));
    lua_pop(L, 1);

    lua_getglobal(L, "axisValue");
    lua_pushvalue(L, -2);
    lua_pushinteger(L, 1);
    CHECK(lua_pcall(L, 2, 1, 0) == LUA_OK);
    CHECK(lua_tonumber(L, -1) == Approx(0.25f));
    lua_pop(L, 2);
}

TEST_CASE("GamepadEvent unknown mapping clears semantic buttons", "[scripting]")
{
    ScriptingTest vm("function southPressed(g)\n"
                     "  return g.south\n"
                     "end\n");

    lua_State* L = vm.state();

    GamepadSnapshot snap{};
    snap.mapping = GamepadMappingKind::unknown;
    snap.buttonMask = uint64_t(1)
                      << static_cast<unsigned>(StandardGamepadButton::south);

    lua_newrive<ScriptedGamepadConnected>(L, snap);

    lua_getglobal(L, "southPressed");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
    CHECK(!lua_toboolean(L, -1));
    lua_pop(L, 2);
}

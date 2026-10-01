/*
 * Copyright 2026 Rive
 */

// Pointer events dispatched to a scripted drawable carry the listener type and
// timestamp of the event, not the ScriptedPointerEvent defaults (which read as
// "pointerEnter" and 0).

#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/scripted/scripted_drawable.hpp"

#include <string>

using namespace rive;

namespace
{
// Stubs addScriptedDirt so the test doesn't need a full artboard graph.
class PointerTypeScriptedDrawable : public ScriptedDrawable
{
public:
    bool addScriptedDirt(ComponentDirt value, bool recurse = false) override
    {
        return true;
    }
};

constexpr const char* pointerTypeScript =
    R"(type MyDrawing = {}
local lastHandler = ''
local lastType = ''
local lastTimeStamp = -1

local function record(handler: string, event: PointerEvent)
  lastHandler = handler
  lastType = event.type
  lastTimeStamp = event.timeStamp
end

function init(self: MyDrawing, context: Context): boolean
  return true
end

function pointerDown(self: MyDrawing, event: PointerEvent)
  record('pointerDown', event)
end

function pointerMove(self: MyDrawing, event: PointerEvent)
  record('pointerMove', event)
end

function pointerUp(self: MyDrawing, event: PointerEvent)
  record('pointerUp', event)
end

function pointerExit(self: MyDrawing, event: PointerEvent)
  record('pointerExit', event)
end

function getLastHandler(): string
  return lastHandler
end

function getLastType(): string
  return lastType
end

function getLastTimeStamp(): number
  return lastTimeStamp
end

return function(): Node<MyDrawing>
  return {
    init = init,
    pointerDown = pointerDown,
    pointerMove = pointerMove,
    pointerUp = pointerUp,
    pointerExit = pointerExit,
  }
end
)";

std::string readString(lua_State* L, const char* getter)
{
    lua_getglobal(L, getter);
    REQUIRE(lua_pcall(L, 0, 1, 0) == LUA_OK);
    std::string value = lua_tostring(L, -1);
    lua_pop(L, 1);
    return value;
}

float readNumber(lua_State* L, const char* getter)
{
    lua_getglobal(L, getter);
    REQUIRE(lua_pcall(L, 0, 1, 0) == LUA_OK);
    float value = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);
    return value;
}
} // namespace

TEST_CASE("scripted drawable pointer events report their type and timestamp",
          "[scripting]")
{
    ScriptingTest vm(pointerTypeScript);
    lua_State* L = vm.state();

    PointerTypeScriptedDrawable drawable;
    // wantsPointerDown | wantsPointerMove | wantsPointerUp | wantsPointerExit
    drawable.implementedMethods((1 << 3) | (1 << 4) | (1 << 5) | (1 << 6));
    drawable.setAsset(make_rcp<ScriptAsset>());
    REQUIRE(drawable.ensureScriptInitialized(vm.vm(), refTopFunction(L)));

    HitScriptedDrawable hit(&drawable, nullptr);

    hit.processEvent(Vec2D(1.0f, 1.0f),
                     ListenerType::down,
                     true,
                     1.5f,
                     0,
                     PointerButton::primary);
    CHECK(readString(L, "getLastHandler") == "pointerDown");
    CHECK(readString(L, "getLastType") == "pointerDown");
    CHECK(readNumber(L, "getLastTimeStamp") == 1.5f);

    hit.processEvent(Vec2D(2.0f, 1.0f),
                     ListenerType::move,
                     true,
                     2.25f,
                     0,
                     PointerButton::primary);
    CHECK(readString(L, "getLastHandler") == "pointerMove");
    CHECK(readString(L, "getLastType") == "pointerMove");
    CHECK(readNumber(L, "getLastTimeStamp") == 2.25f);

    hit.processEvent(Vec2D(2.0f, 1.0f),
                     ListenerType::up,
                     true,
                     3.0f,
                     0,
                     PointerButton::primary);
    CHECK(readString(L, "getLastHandler") == "pointerUp");
    CHECK(readString(L, "getLastType") == "pointerUp");
    CHECK(readNumber(L, "getLastTimeStamp") == 3.0f);

    // An occluded target is dispatched to pointerExit whatever the incoming
    // event was, and the type follows the handler.
    hit.processEvent(Vec2D(2.0f, 1.0f),
                     ListenerType::move,
                     false,
                     4.0f,
                     0,
                     PointerButton::primary);
    CHECK(readString(L, "getLastHandler") == "pointerExit");
    CHECK(readString(L, "getLastType") == "pointerExit");
    CHECK(readNumber(L, "getLastTimeStamp") == 4.0f);
}

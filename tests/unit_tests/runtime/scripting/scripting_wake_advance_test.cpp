/*
 * Copyright 2026 Rive
 */

// A scripted drawable whose advance() has gone idle (returned false) must be
// re-armed when an input event is dispatched to it: the handler may have
// changed state (e.g. fired a trigger on a scripted artboard) that only the
// next scriptAdvance can consume — before the end-of-frame detached view
// model reset wipes it. See rive-app/rive#13052.

#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/scripted/scripted_drawable.hpp"

using namespace rive;

namespace
{
// Stubs addScriptedDirt so the test doesn't need a full artboard graph.
class WakeTestScriptedDrawable : public ScriptedDrawable
{
public:
    bool addScriptedDirt(ComponentDirt value, bool recurse = false) override
    {
        return true;
    }
};

int readCounter(lua_State* L, const char* getter)
{
    lua_getglobal(L, getter);
    REQUIRE(lua_pcall(L, 0, 1, 0) == LUA_OK);
    int value = (int)lua_tonumber(L, -1);
    lua_pop(L, 1);
    return value;
}

constexpr const char* wakeScript =
    R"(type MyDrawing = {}
local advanceCount = 0
local pointerDownCount = 0
local keyCount = 0

function init(self: MyDrawing, context: Context): boolean
  return true
end

function advance(self: MyDrawing, seconds: number): boolean
  advanceCount += 1
  return false -- idle immediately
end

function pointerDown(self: MyDrawing, event: PointerEvent)
  pointerDownCount += 1
end

function keyboardEvent(self: MyDrawing, event: KeyboardEvent): boolean
  keyCount += 1
  return false
end

function getAdvanceCount(): number
  return advanceCount
end

function getPointerDownCount(): number
  return pointerDownCount
end

function getKeyCount(): number
  return keyCount
end

return function(): Node<MyDrawing>
  return {
    init = init,
    advance = advance,
    pointerDown = pointerDown,
    keyboardEvent = keyboardEvent,
  }
end
)";

constexpr AdvanceFlags advanceFlags = AdvanceFlags::Animate |
                                      AdvanceFlags::NewFrame |
                                      AdvanceFlags::AdvanceNested;

// Runs the script's advance once (returns false), then verifies the loop is
// parked: further advances no longer reach the script.
void parkAdvanceLoop(WakeTestScriptedDrawable& drawable, lua_State* L)
{
    int before = readCounter(L, "getAdvanceCount");
    drawable.advanceComponent(0.016f, advanceFlags);
    CHECK(readCounter(L, "getAdvanceCount") == before + 1);
    drawable.advanceComponent(0.016f, advanceFlags);
    CHECK(readCounter(L, "getAdvanceCount") == before + 1);
}
} // namespace

TEST_CASE("pointer event re-arms an idle scripted drawable's advance loop",
          "[scripting]")
{
    ScriptingTest vm(wakeScript);
    lua_State* L = vm.state();

    WakeTestScriptedDrawable drawable;
    // advances (1 << 0) + wantsPointerDown (1 << 3)
    drawable.implementedMethods((1 << 0) | (1 << 3));
    drawable.setAsset(make_rcp<ScriptAsset>());
    REQUIRE(drawable.ensureScriptInitialized(vm.vm()));

    parkAdvanceLoop(drawable, L);

    // Dispatch a pointer event through the hit component.
    HitScriptedDrawable hit(&drawable, nullptr);
    hit.processEvent(Vec2D(1.0f, 1.0f), ListenerType::down, true, 0.0f, 0);
    CHECK(readCounter(L, "getPointerDownCount") == 1);

    // The event re-armed the loop: the next frame advances the script again.
    drawable.advanceComponent(0.016f, advanceFlags);
    CHECK(readCounter(L, "getAdvanceCount") == 2);
}

TEST_CASE("keyboard event re-arms an idle scripted drawable's advance loop",
          "[scripting]")
{
    ScriptingTest vm(wakeScript);
    lua_State* L = vm.state();

    WakeTestScriptedDrawable drawable;
    // advances (1 << 0) + wantsKeyboardInput (1 << 16)
    drawable.implementedMethods((1 << 0) | (1 << 16));
    drawable.setAsset(make_rcp<ScriptAsset>());
    REQUIRE(drawable.ensureScriptInitialized(vm.vm()));

    parkAdvanceLoop(drawable, L);

    drawable.keyInput(Key::a, KeyModifiers::none, true, false);
    CHECK(readCounter(L, "getKeyCount") == 1);

    drawable.advanceComponent(0.016f, advanceFlags);
    CHECK(readCounter(L, "getAdvanceCount") == 2);
}

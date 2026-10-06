/*
 * Copyright 2026 Rive
 */

#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/scripted/scripted_drawable.hpp"

#include <string>

using namespace rive;

namespace
{
// Stubs addScriptedDirt so the test doesn't need a full artboard graph.
class ScrollScriptedDrawable : public ScriptedDrawable
{
public:
    bool addScriptedDirt(ComponentDirt value, bool recurse = false) override
    {
        return true;
    }
};

constexpr int wantsPointerScrollBit = 1 << 21;

constexpr const char* scrollScript =
    R"(type MyMap = {}
local claim = true
local last = ''

function init(self: MyMap, context: Context): boolean
  return true
end

function pointerScroll(self: MyMap, event: ScrollEvent)
  last = string.format('%d %g,%g %g,%g %s %s %g',
    event.id,
    event.position.x, event.position.y,
    event.delta.x, event.delta.y,
    event.phase,
    tostring(event.precise),
    event.timeStamp)
  if claim then
    event:hit()
  end
end

function getLast(): string
  return last
end

function setClaim(value: boolean)
  claim = value
end

return function(): Node<MyMap>
  return {
    init = init,
    pointerScroll = pointerScroll,
  }
end
)";

std::string readLast(lua_State* L)
{
    lua_getglobal(L, "getLast");
    REQUIRE(lua_pcall(L, 0, 1, 0) == LUA_OK);
    std::string value = lua_tostring(L, -1);
    lua_pop(L, 1);
    return value;
}

void setClaim(lua_State* L, bool value)
{
    lua_getglobal(L, "setClaim");
    lua_pushboolean(L, value);
    REQUIRE(lua_pcall(L, 1, 0, 0) == LUA_OK);
}

ScrollEvent trackpad(ScrollPhase phase, float dx, float dy)
{
    ScrollEvent event;
    event.delta = Vec2D(dx, dy);
    event.phase = phase;
    event.precise = true;
    return event;
}

ScrollEvent wheel(float dy)
{
    ScrollEvent event;
    event.delta = Vec2D(0.0f, dy);
    return event;
}

constexpr AdvanceFlags frame = AdvanceFlags::Animate | AdvanceFlags::NewFrame |
                               AdvanceFlags::AdvanceNested;
} // namespace

TEST_CASE("pointerScroll receives the event in the drawable's space",
          "[scripting]")
{
    ScriptingTest vm(scrollScript);
    lua_State* L = vm.state();

    ScrollScriptedDrawable drawable;
    drawable.implementedMethods(wantsPointerScrollBit);
    drawable.setAsset(make_rcp<ScriptAsset>());
    REQUIRE(drawable.ensureScriptInitialized(vm.vm(), refTopFunction(L)));
    // Scaled by 2 and moved, so the delta sees the scale but not the move.
    drawable.mutableWorldTransform() = Mat2D(2, 0, 0, 2, 10, 20);

    HitScriptedDrawable hit(&drawable, nullptr);

    auto result = hit.processScroll(Vec2D(30.0f, 40.0f),
                                    trackpad(ScrollPhase::update, 4.0f, -6.0f),
                                    1.5f,
                                    3);
    CHECK(result == HitResult::hitOpaque);
    CHECK(readLast(L) == "3 10,10 2,-3 update true 1.5");

    hit.processScroll(Vec2D(30.0f, 40.0f), wheel(-10.0f), 2.0f, 0);
    CHECK(readLast(L) == "0 10,10 0,-5 update false 2");
}

TEST_CASE("a claimed trackpad gesture latches until it ends or goes quiet",
          "[scripting]")
{
    ScriptingTest vm(scrollScript);
    lua_State* L = vm.state();

    ScrollScriptedDrawable drawable;
    drawable.implementedMethods(wantsPointerScrollBit);
    drawable.setAsset(make_rcp<ScriptAsset>());
    REQUIRE(drawable.ensureScriptInitialized(vm.vm(), refTopFunction(L)));

    HitScriptedDrawable hit(&drawable, nullptr);
    Vec2D at(5.0f, 5.0f);

    hit.processScroll(at, trackpad(ScrollPhase::begin, 0, 0), 0, 0);
    CHECK(hit.scrollGestureActive());
    hit.processScroll(at, trackpad(ScrollPhase::update, 0, -8), 0, 0);
    CHECK(hit.scrollGestureActive());
    hit.processScroll(at, trackpad(ScrollPhase::end, 0, 0), 0, 0);
    CHECK(!hit.scrollGestureActive());

    // Momentum reports no end, so its latch closes by going quiet.
    hit.processScroll(at, trackpad(ScrollPhase::momentum, 0, -4), 0, 0);
    CHECK(hit.scrollGestureActive());
    drawable.advanceComponent(0.05f, frame);
    CHECK(hit.scrollGestureActive());
    hit.processScroll(at, trackpad(ScrollPhase::momentum, 0, -2), 0, 0);
    drawable.advanceComponent(0.05f, frame);
    CHECK(hit.scrollGestureActive());
    drawable.advanceComponent(0.06f, frame);
    CHECK(!hit.scrollGestureActive());

    hit.processScroll(at, trackpad(ScrollPhase::momentum, 0, -1), 0, 0);
    hit.processScroll(at, trackpad(ScrollPhase::inertiaCancel, 0, 0), 0, 0);
    CHECK(!hit.scrollGestureActive());

    hit.processScroll(at, trackpad(ScrollPhase::begin, 0, 0), 0, 0);
    hit.cancelScroll();
    CHECK(!hit.scrollGestureActive());

    hit.processScroll(at, wheel(-10.0f), 0, 0);
    CHECK(!hit.scrollGestureActive());
}

TEST_CASE("a script that does not claim the scroll lets it pass", "[scripting]")
{
    ScriptingTest vm(scrollScript);
    lua_State* L = vm.state();

    ScrollScriptedDrawable drawable;
    drawable.implementedMethods(wantsPointerScrollBit);
    drawable.setAsset(make_rcp<ScriptAsset>());
    REQUIRE(drawable.ensureScriptInitialized(vm.vm(), refTopFunction(L)));
    setClaim(L, false);

    HitScriptedDrawable hit(&drawable, nullptr);
    auto result = hit.processScroll(Vec2D(1.0f, 2.0f),
                                    trackpad(ScrollPhase::begin, 0, 0),
                                    0,
                                    0);
    CHECK(result == HitResult::none);
    CHECK(readLast(L) == "0 1,2 0,0 begin true 0");
    CHECK(!hit.scrollGestureActive());

    // Once claimed, the gesture stays even through events left unclaimed.
    setClaim(L, true);
    hit.processScroll(Vec2D(1.0f, 2.0f),
                      trackpad(ScrollPhase::begin, 0, 0),
                      0,
                      0);
    setClaim(L, false);
    result = hit.processScroll(Vec2D(1.0f, 2.0f),
                               trackpad(ScrollPhase::update, 0, -4),
                               0,
                               0);
    CHECK(result == HitResult::none);
    CHECK(hit.scrollGestureActive());
}

TEST_CASE("a script without pointerScroll never wants scroll", "[scripting]")
{
    ScriptingTest vm(scrollScript);
    lua_State* L = vm.state();

    ScrollScriptedDrawable drawable;
    // pointerDown only: scroll must stay free for the views beneath.
    drawable.implementedMethods(1 << 3);
    drawable.setAsset(make_rcp<ScriptAsset>());
    REQUIRE(drawable.ensureScriptInitialized(vm.vm(), refTopFunction(L)));

    HitScriptedDrawable hit(&drawable, nullptr);
    CHECK(!hit.wantsScroll(Vec2D(1.0f, 1.0f), wheel(-10.0f)));
    CHECK(!hit.hasScrollTarget(Vec2D(1.0f, 1.0f)));
}

TEST_CASE("ScrollEvent.new defaults to a wheel update", "[scripting]")
{
    ScriptingTest vm(
        "local ev = ScrollEvent.new(3, Vector.xy(1, 2), Vector.xy(0, -4))\n"
        "return string.format('%d %g,%g %g,%g %s %s %g', ev.id,\n"
        "  ev.position.x, ev.position.y, ev.delta.x, ev.delta.y, ev.phase,\n"
        "  tostring(ev.precise), ev.timeStamp)");
    CHECK(std::string(lua_tostring(vm.state(), -1)) ==
          "3 1,2 0,-4 update false 0");
}

TEST_CASE("ScrollEvent.new keeps every field it is given", "[scripting]")
{
    ScriptingTest vm("local ev = ScrollEvent.new(1, Vector.xy(5, 6),\n"
                     "  Vector.xy(7, 8), 'momentum', true, 2.5)\n"
                     "return string.format('%s %s %g', ev.phase,\n"
                     "  tostring(ev.precise), ev.timeStamp)");
    CHECK(std::string(lua_tostring(vm.state(), -1)) == "momentum true 2.5");
}

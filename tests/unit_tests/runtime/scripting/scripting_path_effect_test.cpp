#include <rive/file.hpp>
#include <rive/node.hpp>
#include "rive/animation/state_machine_instance.hpp"
#include "rive/nested_artboard.hpp"
#include "utils/serializing_factory.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive_file_reader.hpp"
#include "scripting_test_utilities.hpp"
#include <cstdio>
#include <catch.hpp>

using namespace rive;

TEST_CASE("Reusing a path in multiple passes works correctly", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/reuse_path_in_effect.riv", &silver);

    auto artboard = file->artboardDefault();
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    auto vmi = file->createDefaultViewModelInstance(artboard.get());

    stateMachine->bindViewModelInstance(vmi);
    auto renderer = silver.makeRenderer();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    CHECK(silver.matches("reuse_path_in_effect"));
}

TEST_CASE("A clip follows the path effect on its source's fill", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/scripted_path_effect_clip.riv", &silver);

    auto artboard = file->artboardDefault();
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    auto vmi = file->createDefaultViewModelInstance(artboard.get());

    stateMachine->bindViewModelInstance(vmi);
    auto renderer = silver.makeRenderer();
    stateMachine->advanceAndApply(0.0f);
    artboard->draw(renderer.get());

    // Multiple frames: a single one passes even if the clip resolves the
    // effect once and then freezes, which is how this broke in the editor.
    for (int i = 0; i < 60; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(1.0f / 60.0f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("scripted_path_effect_clip"));
}

// callPathEffectUpdate reads whatever update() returned off the Lua stack.
// Scripts hand back either a Path or a PathData (separate tags, same
// geometry), and anything else has to resolve to nullptr so the seam reports
// "no geometry" instead of dereferencing a bad pointer.
TEST_CASE("lua_topathdata accepts Path and PathData and rejects the rest",
          "[scripting]")
{
    ScriptingTest vm("return function() return {} end");
    lua_State* L = vm.state();
    auto top = lua_gettop(L);

    RawPath source;
    source.moveTo(0, 0);
    source.lineTo(10, 0);

    lua_newrive<ScriptedPathData>(L, &source);
    CHECK(lua_topathdata(L, -1) != nullptr);
    CHECK(lua_topathdata(L, -1)->rawPath.verbs().size() == 2);
    lua_pop(L, 1);

    lua_newrive<ScriptedPath>(L, &source);
    CHECK(lua_topathdata(L, -1) != nullptr);
    CHECK(lua_topathdata(L, -1)->rawPath.verbs().size() == 2);
    lua_pop(L, 1);

    // The shapes a script returns when it forgets to return geometry.
    lua_pushnil(L);
    CHECK(lua_topathdata(L, -1) == nullptr);
    lua_pop(L, 1);

    lua_pushnumber(L, 42);
    CHECK(lua_topathdata(L, -1) == nullptr);
    lua_pop(L, 1);

    lua_newtable(L);
    CHECK(lua_topathdata(L, -1) == nullptr);
    lua_pop(L, 1);

    // An unrelated rive userdata: non-null to lua_touserdata, but not a path.
    lua_newrive<ScriptedDataValueNumber>(L, L, 1.0f);
    CHECK(lua_touserdata(L, -1) != nullptr);
    CHECK(lua_topathdata(L, -1) == nullptr);
    lua_pop(L, 1);

    CHECK(lua_gettop(L) == top);
}

// The mirror of the above for the data-converter seam.
TEST_CASE("lua_todatavalue accepts the four DataValue kinds only",
          "[scripting]")
{
    ScriptingTest vm("return function() return {} end");
    lua_State* L = vm.state();
    auto top = lua_gettop(L);

    lua_newrive<ScriptedDataValueNumber>(L, L, 3.0f);
    REQUIRE(lua_todatavalue(L, -1) != nullptr);
    CHECK(lua_todatavalue(L, -1)->isNumber());
    lua_pop(L, 1);

    lua_newrive<ScriptedDataValueString>(L, L, "hi");
    REQUIRE(lua_todatavalue(L, -1) != nullptr);
    CHECK(lua_todatavalue(L, -1)->isString());
    lua_pop(L, 1);

    lua_newrive<ScriptedDataValueBoolean>(L, L, true);
    REQUIRE(lua_todatavalue(L, -1) != nullptr);
    CHECK(lua_todatavalue(L, -1)->isBoolean());
    lua_pop(L, 1);

    lua_newrive<ScriptedDataValueColor>(L, L, 0xFF00FF00);
    REQUIRE(lua_todatavalue(L, -1) != nullptr);
    CHECK(lua_todatavalue(L, -1)->isColor());
    lua_pop(L, 1);

    lua_pushnil(L);
    CHECK(lua_todatavalue(L, -1) == nullptr);
    lua_pop(L, 1);

    lua_pushnumber(L, 42);
    CHECK(lua_todatavalue(L, -1) == nullptr);
    lua_pop(L, 1);

    RawPath source;
    source.moveTo(0, 0);
    lua_newrive<ScriptedPathData>(L, &source);
    CHECK(lua_touserdata(L, -1) != nullptr);
    CHECK(lua_todatavalue(L, -1) == nullptr);
    lua_pop(L, 1);

    CHECK(lua_gettop(L) == top);
}


#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/animation/state_machine_instance.hpp"
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

TEST_CASE("can access nodes from artboards", "[scripting]")
{
    ScriptingTest vm("function getNode(artboard:Artboard):Node?\n"
                     "  return artboard:node('muzzle')\n"
                     "end\n"
                     "function getNodeX(artboard:Artboard):number\n"
                     "  return artboard:node('muzzle').x\n"
                     "end\n"
                     "function getNodeY(artboard:Artboard):number\n"
                     "  return artboard:node('muzzle').y\n"
                     "end\n"
                     "function getScaleX(artboard:Artboard):number\n"
                     "  return artboard:node('muzzle').scaleX\n"
                     "end\n"
                     "function getScaleY(artboard:Artboard):number\n"
                     "  return artboard:node('muzzle').scaleY\n"
                     "end\n"
                     "function decompose(artboard:Artboard)\n"
                     "  artboard:node('muzzle'):decompose(Mat2D.identity())\n"
                     "end\n"
                     "function getChildren(artboard:Artboard):number\n"
                     "  return #(artboard:node('muzzle').children)\n"
                     "end\n"
                     "function getWeaponChildren(artboard:Artboard):number\n"
                     "  return #(artboard:node('Weapon').children)\n"
                     "end\n"
                     "function getParent(artboard:Artboard):Node?\n"
                     "  return artboard:node('muzzle').parent\n"
                     "end\n");
    lua_State* L = vm.state();
    auto file = ReadRiveFile("assets/joel_v3.riv", vm.serializer());
    auto artboard = file->artboard("Character");
    REQUIRE(artboard != nullptr);
    lua_newrive<ScriptedArtboard>(L, file, artboard->instance());

    lua_getglobal(L, "getNode");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
    CHECK(lua_isuserdata(L, -1));
    lua_pop(L, 1);

    lua_getglobal(L, "getNodeX");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
    CHECK(lua_tonumber(L, -1) == 203.0);
    lua_pop(L, 1);

    lua_getglobal(L, "getNodeY");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
    CHECK(lua_tonumber(L, -1) == 0.0);
    lua_pop(L, 1);

    lua_getglobal(L, "getScaleX");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
    CHECK(lua_tonumber(L, -1) == Approx(1.2500029802));
    lua_pop(L, 1);

    lua_getglobal(L, "getScaleY");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
    CHECK(lua_tonumber(L, -1) == Approx(1.2500029802));
    lua_pop(L, 1);

    lua_getglobal(L, "decompose");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 0, 0) == LUA_OK);

    lua_getglobal(L, "getNodeX");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
    CHECK(lua_tonumber(L, -1) == 0.0);
    lua_pop(L, 1);

    lua_getglobal(L, "getNodeY");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
    CHECK(lua_tonumber(L, -1) == 0.0);
    lua_pop(L, 1);

    lua_getglobal(L, "getScaleX");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
    CHECK(lua_tonumber(L, -1) == 1);
    lua_pop(L, 1);

    lua_getglobal(L, "getScaleY");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
    CHECK(lua_tonumber(L, -1) == 1);
    lua_pop(L, 1);

    lua_getglobal(L, "getChildren");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
    CHECK(lua_tonumber(L, -1) == 0);
    lua_pop(L, 1);

    lua_getglobal(L, "getParent");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
    CHECK(lua_isuserdata(L, -1));
    lua_pop(L, 1);

    lua_getglobal(L, "getWeaponChildren");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
    CHECK(lua_tonumber(L, -1) == 9);
    lua_pop(L, 1);
}

TEST_CASE("can add artboard to path", "[scripting]")
{
    ScriptingTest vm("function addToPath(artboard:Artboard)\n"
                     "  local path:Path = Path.new()\n"
                     "  artboard:addToPath(path)\n"
                     "end\n"
                     "function addToPathWithTransform(artboard:Artboard)\n"
                     "  local path:Path = Path.new()\n"
                     "  artboard:addToPath(path, Mat2D.identity())\n"
                     "end\n");
    lua_State* L = vm.state();
    auto file = ReadRiveFile("assets/joel_v3.riv", vm.serializer());
    auto artboard = file->artboard("Character");
    REQUIRE(artboard != nullptr);
    lua_newrive<ScriptedArtboard>(L, file, artboard->instance());

    lua_getglobal(L, "addToPath");
    lua_pushvalue(L, -2);
    CHECK(lua_pcall(L, 1, 1, 0) == LUA_OK);
}

TEST_CASE("script instances Artboard input", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/script_artboard_test.riv", &silver);

    auto artboard = file->artboardNamed("Artboard");

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = 60;
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("script_artboards"));
}
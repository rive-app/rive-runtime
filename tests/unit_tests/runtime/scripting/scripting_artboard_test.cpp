
#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive_file_reader.hpp"

using namespace rive;

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
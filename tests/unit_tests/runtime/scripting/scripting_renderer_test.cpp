
#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"

using namespace rive;

TEST_CASE("can call renderer", "[scripting]")
{
    ScriptingTest vm(
        "local storedRenderer:Renderer\n"
        "function render(renderer:Renderer):()\n"
        "  storedRenderer = renderer\n" // doing a naughty thing on purpose
        "  local path:Path = Path.new()\n"
        "  local paint:Paint = Paint.new()\n"
        "  renderer:drawPath(path, paint)\n"
        "end\n"
        "function afterwards(): ()\n"
        "  storedRenderer:save()\n" // trying to use the renderer outside of the
                                    // callback
        "end");
    lua_State* L = vm.state();
    lua_getglobal(L, "render");
    auto renderer = vm.factory()->makeRenderer();
    auto scriptedRenderer = lua_newrive<ScriptedRenderer>(L, renderer.get());
    CHECK(lua_pcall(L, 1, 0, 0) == LUA_OK);
    CHECK(scriptedRenderer->end());

    lua_getglobal(L, "afterwards");
    CHECK(lua_pcall(L, 0, 0, 0) == LUA_ERRRUN);
    CHECK(lua_tostring(L, -1) ==
          std::string(
              "[string \"test_source\"]:9: Renderer is no longer valid."));
}

TEST_CASE("renderer checks its balanced", "[scripting]")
{
    ScriptingTest vm("function render(renderer:Renderer):()\n"
                     "  local path:Path = Path.new()\n"
                     "  local paint:Paint = Paint.new()\n"
                     "  renderer:save()\n"
                     "  renderer:drawPath(path, paint)\n"
                     "  renderer:save()\n"
                     "end\n");
    lua_State* L = vm.state();
    lua_getglobal(L, "render");
    auto renderer = vm.factory()->makeRenderer();
    auto scriptedRenderer = lua_newrive<ScriptedRenderer>(L, renderer.get());
    CHECK(lua_pcall(L, 1, 0, 0) == LUA_OK);
    CHECK(!scriptedRenderer->end());
}

TEST_CASE("renderer can draw an oval", "[scripting]")
{
    ScriptingTest vm(
        R"(function addOval(path: Path, x: number, y: number, width: number, height: number)
	local c: number = 0.5519150244935105707435627
	local unit: { Vec2D } = {
		Vec2D.xy(1, 0),
		Vec2D.xy(1, c),
		Vec2D.xy(c, 1), -- quadrant 1 ( 4:30)
		Vec2D.xy(0, 1),
		Vec2D.xy(-c, 1),
		Vec2D.xy(-1, c), -- quadrant 2 ( 7:30)
		Vec2D.xy(-1, 0),
		Vec2D.xy(-1, -c),
		Vec2D.xy(-c, -1), -- quadrant 3 (10:30)
		Vec2D.xy(0, -1),
		Vec2D.xy(c, -1),
		Vec2D.xy(1, -c), -- quadrant 4 ( 1:30)
		Vec2D.xy(1, 0),
	}

	local dx: number = x - width / 2
	local dy: number = y - height / 2
	local sx: number = width * 0.5
	local sy: number = height * 0.5

	local map = function(p: Vec2D): Vec2D
		return Vec2D.xy(p.x * sx + dx, p.y * sy + dy)
	end
	path:moveTo(map(unit[1]))
	for i = 1, 12, 3 do
		path:cubicTo(map(unit[i + 1]), map(unit[i + 2]), map(unit[i + 3]))
	end
	path:close()
end

function render(renderer: Renderer): ()
	local path: Path = Path.new()
	local paint: Paint = Paint.with({color=0xFFFF0000, feather=20})

	addOval(path, 600, 500, 100, 180)
	renderer:drawPath(path, paint)
end
)");
    lua_State* L = vm.state();
    lua_getglobal(L, "render");
    auto renderer = vm.factory()->makeRenderer();
    auto scriptedRenderer = lua_newrive<ScriptedRenderer>(L, renderer.get());
    CHECK(lua_pcall(L, 1, 0, 0) == LUA_OK);
    CHECK(scriptedRenderer->end());

    CHECK(vm.factory()->matches("scripted_oval"));
}

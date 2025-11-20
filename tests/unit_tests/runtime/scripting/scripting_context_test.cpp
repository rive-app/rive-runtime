
#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"

using namespace rive;

TEST_CASE("Scripted Context markNeedsUpdate works", "[scripting]")
{
    ScriptingTest vm(
        R"(

-- Called once when the script initializes.
function init(self: MyNode, context: Context): boolean
  context:markNeedsUpdate()
  return true
end

)");
    ScriptedObjectTest scriptedObjectTest;
    lua_State* L = vm.state();
    auto top = lua_gettop(L);
    {
        lua_getglobal(L, "init");
        lua_pushvalue(L, -2);
        lua_newrive<ScriptedContext>(L, &scriptedObjectTest);
        CHECK(lua_pcall(L, 2, 1, 0) == LUA_OK);
        rive_lua_pop(L, 1);
        CHECK(top == lua_gettop(L));
        CHECK(scriptedObjectTest.needsUpdate());
    }
}

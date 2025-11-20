
#include "catch.hpp"
#include "scripting_test_utilities.hpp"

using namespace rive;

TEST_CASE("pointer event can be constructed", "[scripting]")
{
    CHECK(lua_tonumber(
              ScriptingTest("local ev = PointerEvent.new(4, Vector.origin())\n"
                            "return ev.id")
                  .state(),
              -1) == 4.0f);
}

TEST_CASE("pointer event can be constructed with a specified position",
          "[scripting]")
{
    CHECK(lua_tonumber(
              ScriptingTest("local ev = PointerEvent.new(4, Vector.xy(11,12))\n"
                            "return ev.position.x")
                  .state(),
              -1) == 11.0f);
}

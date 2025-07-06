
#include "catch.hpp"
#include "scripting_test_utilities.hpp"

using namespace rive;

TEST_CASE("vec2d can be constructed", "[scripting]")
{
    CHECK(lua_tonumber(ScriptingTest("local _vec = Vec2D(1,2)\n"
                                     "return _vec.x")
                           .state(),
                       -1) == 1.0f);

    CHECK(lua_tonumber(ScriptingTest("local _vec = Vec2D(1,2)\n"
                                     "return _vec.y")
                           .state(),
                       -1) == 2.0f);

    CHECK(lua_tonumber(ScriptingTest("local _vec = Vec2D(33)\n"
                                     "return _vec.x")
                           .state(),
                       -1) == 33.0f);

    CHECK(lua_tonumber(ScriptingTest("local _vec = Vec2D(33)\n"
                                     "return _vec.y")
                           .state(),
                       -1) == 33.0f);

    CHECK(lua_tonumber(ScriptingTest("local _vec = Vec2D()\n"
                                     "return _vec.x")
                           .state(),
                       -1) == 0.0f);

    CHECK(lua_tonumber(ScriptingTest("local _vec = Vec2D()\n"
                                     "return _vec.y")
                           .state(),
                       -1) == 0.0f);
}

TEST_CASE("vec2d static methods work", "[scripting]")
{
    CHECK(
        lua_tonumber(
            ScriptingTest("return Vec2D.distance(Vec2D(),Vec2D(10,0))").state(),
            -1) == 10.0f);

    CHECK(lua_tonumber(
              ScriptingTest("return Vec2D.distanceSquared(Vec2D(),Vec2D(10,0))")
                  .state(),
              -1) == 100.0f);

    CHECK(lua_tonumber(
              ScriptingTest("return Vec2D.dot(Vec2D(1,0),Vec2D(-1,0))").state(),
              -1) == -1.0f);

    CHECK(lua_tonumber(
              ScriptingTest("return Vec2D.lerp(Vec2D(),Vec2D(1,2), 0.5).x")
                  .state(),
              -1) == 0.5f);

    CHECK(lua_tonumber(
              ScriptingTest("return Vec2D.lerp(Vec2D(),Vec2D(1,2), 0.5).y")
                  .state(),
              -1) == 1.0f);
}

TEST_CASE("vec2d indexing work", "[scripting]")
{
    CHECK(lua_tonumber(ScriptingTest("return Vec2D(19, 27)[1]").state(), -1) ==
          19.0f);

    CHECK(lua_tonumber(ScriptingTest("return Vec2D(19, 27)[2]").state(), -1) ==
          27.0f);
}

TEST_CASE("vec2d methods work", "[scripting]")
{
    CHECK(lua_tonumber(
              ScriptingTest("return Vec2D():distance(Vec2D(10,0))").state(),
              -1) == 10.0f);

    CHECK(lua_tonumber(
              ScriptingTest("return Vec2D():distanceSquared(Vec2D(10,0))")
                  .state(),
              -1) == 100.0f);

    CHECK(lua_tonumber(
              ScriptingTest("return Vec2D(1,0):dot(Vec2D(-1,0))").state(),
              -1) == -1.0f);

    CHECK(lua_tonumber(
              ScriptingTest("return Vec2D():lerp(Vec2D(1,2), 0.5).x").state(),
              -1) == 0.5f);

    CHECK(lua_tonumber(
              ScriptingTest("return Vec2D():lerp(Vec2D(1,2), 0.5).y").state(),
              -1) == 1.0f);
}

TEST_CASE("vec2d meta methods work", "[scripting]")
{
    CHECK(lua_tonumber(ScriptingTest("return -Vec2D(12,13).x").state(), -1) ==
          -12.0f);
    CHECK(lua_tonumber(ScriptingTest("return -Vec2D(12,13).y").state(), -1) ==
          -13.0f);
    CHECK(lua_tonumber(
              ScriptingTest("return (Vec2D(12,13)+Vec2D(2,3)).y").state(),
              -1) == 16.0f);
    CHECK(lua_tonumber(
              ScriptingTest("return (Vec2D(12,13)+Vec2D(2,3)).x").state(),
              -1) == 14.0f);
    CHECK(lua_tonumber(
              ScriptingTest("return (Vec2D(12,13)-Vec2D(2,3)).y").state(),
              -1) == 10.0f);
    CHECK(lua_tonumber(
              ScriptingTest("return (Vec2D(12,13)-Vec2D(2,3)).x").state(),
              -1) == 10.0f);
    CHECK(lua_tonumber(ScriptingTest("return (Vec2D(12,13)*3).x").state(),
                       -1) == 36.0f);
    CHECK(lua_tonumber(ScriptingTest("return (Vec2D(12,13)*3).y").state(),
                       -1) == 39.0f);
    CHECK(lua_tonumber(ScriptingTest("return (Vec2D(12,13)/3).x").state(),
                       -1) == 4.0f);
    CHECK(lua_tonumber(ScriptingTest("return (Vec2D(12,13)/3).y").state(),
                       -1) == Approx(4.3333333f));
    CHECK(
        lua_toboolean(ScriptingTest("return Vec2D(1,2) == Vec2D(2,1)").state(),
                      -1) == 0);
    CHECK(
        lua_toboolean(ScriptingTest("return Vec2D(1,2) == Vec2D(1,2)").state(),
                      -1) == 1);
    CHECK(
        lua_toboolean(ScriptingTest("return Vec2D(1,2) ~= Vec2D(2,1)").state(),
                      -1) == 1);
    CHECK(
        lua_toboolean(ScriptingTest("return Vec2D(1,2) ~= Vec2D(1,2)").state(),
                      -1) == 0);
}

static int lua_callback(lua_State* L)
{

    unsigned index = lua_tounsignedx(L, lua_upvalueindex(1), nullptr);
    fprintf(stderr, "index from callback upvalue is: %i\n", index);
    return 0;
}

static void* l_alloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
    (void)ud;
    (void)osize; /* not used */
    if (nsize == 0)
    {
        free(ptr);
        return NULL;
    }
    else
        return realloc(ptr, nsize);
}

TEST_CASE("closure test", "[scripting]")
{
    lua_State* L;
    const char* source = "callMyFunc()";
    size_t bytecodeSize = 0;
    char* bytecode = luau_compile(source, strlen(source), NULL, &bytecodeSize);
    REQUIRE(bytecode != nullptr);
    L = lua_newstate(l_alloc, nullptr);
    // lua_setthreaddata(L, nullptr);
    REQUIRE(L != nullptr);
    luaopen_rive(L);

    lua_pushinteger(L, 222);
    lua_pushcclosurek(L, lua_callback, "callMyFunc", 1, nullptr);
    lua_setglobal(L, "callMyFunc");

    luaL_sandbox(L);
    luaL_sandboxthread(L);

    CHECK(luau_load(L, "test_source", bytecode, bytecodeSize, 0) == LUA_OK);
    free(bytecode);

    auto result = lua_pcall(L, 0, 0, 0);
    CHECK(result == LUA_OK);
    if (result != LUA_OK)
    {
        auto error = lua_tostring(L, -1);
        fprintf(stderr, "  %s\n", error);
    }

    lua_close(L);
}

#include "catch.hpp"
#include "scripting_test_utilities.hpp"

using namespace rive;

TEST_CASE("scripting require", "[scripting]")
{
    CHECK(
        std::string(lua_tostring(
            ScriptingTest("local util = require('utilities')\n"
                          "local util2 = require('util2')\n"
                          "return util.name .. util2()",
                          1,
                          false,
                          {{"utilities", "return { name = 'hello' }"},
                           {"util2", "return function() return ' world'; end"}})
                .state(),
            -1)) == "hello world");
}

TEST_CASE("scripting require with bad code", "[scripting]")
{
    // Utilities should fail to load
    ScriptingTest vm("local util = require('utilities')\n"
                     "return util.name",
                     1,
                     true,
                     {
                         {"utilities", "return { 'name' = 'hello' }"},
                     });
    auto error = lua_tostring(vm.state(), -1);
    CHECK(error == std::string("test_source:1: require could "
                               "not find a script named utilities"));
}

TEST_CASE("scripting require with bad unused module is ok", "[scripting]")
{
    // utilities2 should fail to load
    ScriptingTest vm("local util = require('utilities')\n"
                     "return util.name",
                     1,
                     false,
                     {
                         {"utilities", "return { name = 'hello' }"},
                         {"utilities2", "return { 'name' = 'hello' }"},
                     });
    auto result = lua_tostring(vm.state(), -1);
    CHECK(result == std::string("hello"));
}

TEST_CASE("scripting require removed module works", "[scripting]")
{
    ScriptingTest vm("local util = require('utilities')\n"
                     "return util.name",
                     1,
                     true,
                     {
                         {"utilities", "return { name = 'hello' }"},
                     },
                     false);
    vm.unregisterModule("utilities");
    vm.execute();
    auto result = lua_tostring(vm.state(), -1);
    CHECK(result == std::string("test_source:1: require could not "
                                "find a script named utilities"));
}

TEST_CASE("scripting time values are available", "[scripting]")
{
    ScriptingTest vm("return os.date(\"!%Y-%m-%d %H:%M:%S\",1761608005)\n", 1);
    auto result = lua_tostring(vm.state(), -1);
    CHECK(result == std::string("2025-10-27 23:33:25"));
}

TEST_CASE("buffer API is available", "[scripting]")
{
    // Test buffer creation and basic operations
    ScriptingTest vm("local buf = buffer.create(10)\n"
                     "buffer.writei8(buf, 0, 42)\n"
                     "local value = buffer.readi8(buf, 0)\n"
                     "return value",
                     1);
    lua_Number result = lua_tonumber(vm.state(), -1);
    CHECK(result == 42);
}

TEST_CASE("buffer fromstring and tostring work", "[scripting]")
{
    // Test buffer string conversion
    ScriptingTest vm("local buf = buffer.fromstring('hello')\n"
                     "return buffer.tostring(buf)",
                     1);
    auto result = lua_tostring(vm.state(), -1);
    CHECK(result == std::string("hello"));
}

TEST_CASE("buffer len works", "[scripting]")
{
    // Test buffer length
    ScriptingTest vm("local buf = buffer.create(20)\n"
                     "return buffer.len(buf)",
                     1);
    lua_Number result = lua_tonumber(vm.state(), -1);
    CHECK(result == 20);
}

TEST_CASE("bit32 API is available", "[scripting]")
{
    // Test bit32 bitwise operations
    ScriptingTest vm("local result = bit32.band(5, 3)\n"
                     "return result",
                     1);
    lua_Number result = lua_tonumber(vm.state(), -1);
    CHECK(result == 1); // 5 & 3 = 1
}

TEST_CASE("bit32 bor and bxor work", "[scripting]")
{
    // Test bit32 OR and XOR operations
    ScriptingTest vm("local orResult = bit32.bor(5, 3)\n"
                     "local xorResult = bit32.bxor(5, 3)\n"
                     "return orResult, xorResult",
                     2);
    lua_Number orResult = lua_tonumber(vm.state(), -2);
    lua_Number xorResult = lua_tonumber(vm.state(), -1);
    CHECK(orResult == 7);  // 5 | 3 = 7
    CHECK(xorResult == 6); // 5 ^ 3 = 6
}

TEST_CASE("bit32 shifts work", "[scripting]")
{
    // Test bit32 shift operations
    ScriptingTest vm("local leftShift = bit32.lshift(1, 3)\n"
                     "local rightShift = bit32.rshift(8, 3)\n"
                     "return leftShift, rightShift",
                     2);
    lua_Number leftShift = lua_tonumber(vm.state(), -2);
    lua_Number rightShift = lua_tonumber(vm.state(), -1);
    CHECK(leftShift == 8);  // 1 << 3 = 8
    CHECK(rightShift == 1); // 8 >> 3 = 1
}
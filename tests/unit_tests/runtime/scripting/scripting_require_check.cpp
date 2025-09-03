
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
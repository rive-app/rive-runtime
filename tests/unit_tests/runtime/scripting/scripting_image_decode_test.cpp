
#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/async/work_pool.hpp"

using namespace rive;

// Defined in lua_image_decode.cpp.
extern int context_decodeImage_impl(lua_State* L);

// Wrapper so we can register context_decodeImage_impl as a plain Lua global.
// Shifts args so that arg 1 (the buffer) becomes arg 2, matching the
// context:decodeImage(buf) calling convention.
static int decodeImage_wrapper(lua_State* L)
{
    // context_decodeImage_impl expects self at 1, buffer at 2.
    // Push a dummy nil for self, then move the buffer after it.
    lua_pushnil(L);
    lua_insert(L, 1); // nil is now at index 1, buffer at index 2
    return context_decodeImage_impl(L);
}

// ============================================================================
// decodeImage cancellation
// ============================================================================

TEST_CASE("decodeImage cancel sets promise to Cancelled",
          "[scripting][image_decode]")
{
    ScriptingTest test(
        R"(
        local p = decodeImage(buffer.create(4))
        p:cancel()
        return p:getStatus()
    )",
        1,
        false,
        {},
        false);

    // Register the wrapper before executing.
    lua_pushcfunction(test.state(), decodeImage_wrapper, "decodeImage");
    lua_setglobal(test.state(), "decodeImage");
    test.execute();

    CHECK(std::string(lua_tostring(test.state(), -1)) == "Cancelled");
}

TEST_CASE("decodeImage cancel does not fire andThen",
          "[scripting][image_decode]")
{
    ScriptingTest test(
        R"(
        local called = false
        local p = decodeImage(buffer.create(4))
        p:andThen(function() called = true end)
        p:cancel()
        return called
    )",
        1,
        false,
        {},
        false);

    lua_pushcfunction(test.state(), decodeImage_wrapper, "decodeImage");
    lua_setglobal(test.state(), "decodeImage");
    test.execute();

    CHECK(lua_toboolean(test.state(), -1) == 0);
}

TEST_CASE("decodeImage cancel fires onCancel hook", "[scripting][image_decode]")
{
    ScriptingTest test(
        R"(
        local hookFired = false
        local p = decodeImage(buffer.create(4))
        p:onCancel(function() hookFired = true end)
        p:cancel()
        return hookFired
    )",
        1,
        false,
        {},
        false);

    lua_pushcfunction(test.state(), decodeImage_wrapper, "decodeImage");
    lua_setglobal(test.state(), "decodeImage");
    test.execute();

    CHECK(lua_toboolean(test.state(), -1) != 0);
}

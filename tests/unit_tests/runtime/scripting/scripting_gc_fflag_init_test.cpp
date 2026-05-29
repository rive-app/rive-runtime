#include "catch.hpp"
#include "lua.h"
#include "lualib.h"
#include "Luau/Common.h"
#include "scripting_test_utilities.hpp"

LUAU_FASTFLAG(LuauDirectFieldGet)

using namespace rive;

namespace
{
struct ScopedFFlag
{
    Luau::FValue<bool>& flag;
    const bool saved;
    ScopedFFlag(Luau::FValue<bool>& f, bool newValue) : flag(f), saved(f.value)
    {
        flag.value = newValue;
    }
    ~ScopedFFlag() { flag.value = saved; }
};
} // namespace

// Regression test for the markroot crash that surfaced after the rive_0_36
// luau bump. lua_newstate() conditionally initialized global_State's
// udatadirectfields[] based on FFlag::LuauDirectFieldGet. If the flag was off
// when newstate ran but later flipped on (as Rive's LuauFlagsConfig does at
// editor startup), markroot would read uninitialized garbage and SIGSEGV.
//
// The fix lives in the vendored luau fork at VM/src/lstate.cpp: init is now
// unconditional. This test exercises the flip-after-newstate sequence; the
// RAII guard ensures the global flag is restored even if any step throws.
TEST_CASE("ScriptingVM survives FFlag::LuauDirectFieldGet flip after newstate",
          "[scripting][gc]")
{
    ScopedFFlag guard(FFlag::LuauDirectFieldGet, false);
    ScriptingTest test("return 1", 1, false, {}, false);

    FFlag::LuauDirectFieldGet.value = true;

    lua_State* L = test.state();
    for (int i = 0; i < 64; ++i)
    {
        lua_createtable(L, 0, 0);
        lua_pop(L, 1);
    }
    lua_gc(L, LUA_GCCOLLECT, 0);

    SUCCEED("no crash in markroot after flag flip");
}

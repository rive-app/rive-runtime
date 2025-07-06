#ifndef _RIVE_SCRIPTING_TEST_UTILITIES_HPP_
#define _RIVE_SCRIPTING_TEST_UTILITIES_HPP_

#include "lua.h"
#include "lualib.h"
#include "luacode.h"
#include "rive/lua/rive_lua_libs.hpp"
#include "utils/serializing_factory.hpp"

namespace rive
{
class ScriptingTest
{
public:
    ScriptingTest(const char* source, int numResults = 1)
    {
        m_vm = rivestd::make_unique<ScriptingVM>(&m_factory);
        auto state = m_vm->state();
        size_t bytecodeSize = 0;
        char* bytecode =
            luau_compile(source, strlen(source), NULL, &bytecodeSize);
        REQUIRE(bytecode != nullptr);
        CHECK(luau_load(state, "test_source", bytecode, bytecodeSize, 0) ==
              LUA_OK);
        free(bytecode);

        auto result = lua_pcall(state, 0, numResults, 0);
        CHECK(result == LUA_OK);
        if (result != LUA_OK)
        {
            auto error = lua_tostring(state, -1);
            fprintf(stderr, "  %s\n", error);
        }
    }

    lua_State* state() { return m_vm->state(); }

public:
    SerializingFactory* factory() { return &m_factory; }

private:
    SerializingFactory m_factory;
    std::unique_ptr<ScriptingVM> m_vm;
};
} // namespace rive
#endif

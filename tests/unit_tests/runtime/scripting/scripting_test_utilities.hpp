#ifndef _RIVE_SCRIPTING_TEST_UTILITIES_HPP_
#define _RIVE_SCRIPTING_TEST_UTILITIES_HPP_

#include "lua.h"
#include "lualib.h"
#include "luacode.h"
#include "rive/lua/rive_lua_libs.hpp"
#include "utils/serializing_factory.hpp"
#include <unordered_map>

namespace rive
{
class ScriptingTest
{
public:
    ScriptingTest(const char* source,
                  int numResults = 1,
                  bool errorOk = false,
                  std::unordered_map<std::string, std::string> modules = {})
    {
        m_vm = rivestd::make_unique<ScriptingVM>(&m_factory);

        for (const auto& pair : modules)
        {
            size_t bytecodeSize = 0;
            char* bytecode = luau_compile(pair.second.c_str(),
                                          pair.second.length(),
                                          nullptr,
                                          &bytecodeSize);
            REQUIRE(bytecode != nullptr);
            m_vm->registerModule(
                pair.first.c_str(),
                Span<uint8_t>((uint8_t*)bytecode, bytecodeSize));
            free(bytecode);
        }

        auto state = m_vm->state();
        size_t bytecodeSize = 0;
        char* bytecode =
            luau_compile(source, strlen(source), nullptr, &bytecodeSize);
        REQUIRE(bytecode != nullptr);
        CHECK(luau_load(state, "test_source", bytecode, bytecodeSize, 0) ==
              LUA_OK);
        free(bytecode);

        auto result = lua_pcall(state, 0, numResults, 0);
        if (!errorOk)
        {
            CHECK(result == LUA_OK);
        }
        else if (errorOk && result != LUA_OK)
        {
            return;
        }
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

#ifndef _RIVE_SCRIPTING_TEST_UTILITIES_HPP_
#define _RIVE_SCRIPTING_TEST_UTILITIES_HPP_

#include "lua.h"
#include "lualib.h"
#include "luacode.h"
#include "rive/lua/rive_lua_libs.hpp"
#include "utils/serializing_factory.hpp"
#include "rive/scripted/scripted_object.hpp"
#include <unordered_map>

namespace rive
{
class ScriptingTest : public ScriptingContext
{
private:
    int m_numResults;
    bool m_errorOk;

public:
    ScriptingTest(const char* source,
                  int numResults = 1,
                  bool errorOk = false,
                  std::unordered_map<std::string, std::string> modules = {},
                  bool executeImmediately = true) :
        ScriptingContext(&m_factory)
    {
        m_numResults = numResults;
        m_errorOk = errorOk;
        m_vm = rivestd::make_unique<ScriptingVM>(this);

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

        if (executeImmediately)
        {
            execute();
        }
    }

    void execute()
    {
        auto result = pCall(m_vm->state(), 0, m_numResults);
        if (!m_errorOk)
        {
            CHECK(result == LUA_OK);
        }
        else if (m_errorOk && result != LUA_OK)
        {
            return;
        }
        if (result != LUA_OK)
        {
            auto error = lua_tostring(m_vm->state(), -1);
            fprintf(stderr, "  %s\n", error);
        }
    }

    void unregisterModule(const char* name) { m_vm->unregisterModule(name); }

    lua_State* state() { return m_vm->state(); }

public:
    SerializingFactory* serializer() { return &m_factory; }
    std::vector<std::string> console;
    std::vector<std::string> errors;

    std::string currentLine = "";

    void printBeginLine(lua_State* state) override {}

    void print(Span<const char> data) override
    {
        currentLine += std::string(data.data(), data.size());
    }

    void printEndLine() override
    {
        console.push_back(currentLine);
        currentLine = "";
    }

    void printError(lua_State* state) override
    {
        const char* error = lua_tostring(state, -1);
        errors.push_back(error);
    }

    int pCall(lua_State* state, int nargs, int nresults) override
    {
        return lua_pcall(state, nargs, nresults, 0);
    }

private:
    SerializingFactory m_factory;
    std::unique_ptr<ScriptingVM> m_vm;
};

class ScriptedObjectTest : public ScriptedObject
{
    bool addScriptedDirt(ComponentDirt value, bool recurse = false) override
    {
        return true;
    }
    bool m_markedToUpdate = false;
    uint32_t assetId() override { return 0; }
    ScriptProtocol scriptProtocol() override { return ScriptProtocol::utility; }
    void markNeedsUpdate() override { m_markedToUpdate = true; }
    Component* component() override { return nullptr; }

public:
    bool needsUpdate() { return m_markedToUpdate; }
};
} // namespace rive
#endif

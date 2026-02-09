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
class ScriptingTest;

// A ScriptingContext that delegates callbacks to ScriptingTest
class TestScriptingContext : public ScriptingContext
{
public:
    ScriptingTest* test;

    TestScriptingContext(Factory* factory, ScriptingTest* t) :
        ScriptingContext(factory), test(t)
    {}

    void printBeginLine(lua_State* state) override;
    void print(Span<const char> data) override;
    void printEndLine() override;
    void printError(lua_State* state) override;
    int pCall(lua_State* state, int nargs, int nresults) override;
};

class ScriptingTest
{
private:
    int m_numResults;
    bool m_errorOk;

public:
    ScriptingTest(const char* source,
                  int numResults = 1,
                  bool errorOk = false,
                  std::unordered_map<std::string, std::string> modules = {},
                  bool executeImmediately = true)
    {
        m_numResults = numResults;
        m_errorOk = errorOk;
        auto context =
            rivestd::make_unique<TestScriptingContext>(&m_factory, this);
        m_vm = make_rcp<ScriptingVM>(std::move(context));

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

        auto luaState = m_vm->state();
        size_t bytecodeSize = 0;
        char* bytecode =
            luau_compile(source, strlen(source), nullptr, &bytecodeSize);
        REQUIRE(bytecode != nullptr);
        CHECK(luau_load(luaState, "test_source", bytecode, bytecodeSize, 0) ==
              LUA_OK);
        free(bytecode);

        if (executeImmediately)
        {
            execute();
        }
    }

    void execute()
    {
        auto context = static_cast<TestScriptingContext*>(m_vm->context());
        auto result = context->pCall(m_vm->state(), 0, m_numResults);
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
    ScriptingVM* vm() { return m_vm.get(); }

public:
    SerializingFactory* serializer() { return &m_factory; }
    std::vector<std::string> console;
    std::vector<std::string> errors;

    std::string currentLine = "";

    void print(Span<const char> data)
    {
        currentLine += std::string(data.data(), data.size());
    }

    void printEndLine()
    {
        console.push_back(currentLine);
        currentLine = "";
    }

    void printError(const char* error) { errors.push_back(error); }

private:
    SerializingFactory m_factory;
    rcp<ScriptingVM> m_vm;
};

// Inline implementations for TestScriptingContext
inline void TestScriptingContext::printBeginLine(lua_State* state)
{
    // Tests don't need debug info, just start a new line
}
inline void TestScriptingContext::print(Span<const char> data)
{
    test->print(data);
}
inline void TestScriptingContext::printEndLine() { test->printEndLine(); }
inline void TestScriptingContext::printError(lua_State* state)
{
    const char* error = lua_tostring(state, -1);
    test->printError(error);
}
inline int TestScriptingContext::pCall(lua_State* state,
                                       int nargs,
                                       int nresults)
{
    // Simple pCall without timeout for tests
    return lua_pcall(state, nargs, nresults, 0);
}

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

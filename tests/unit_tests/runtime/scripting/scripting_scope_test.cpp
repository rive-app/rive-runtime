#include "catch.hpp"
#include "rive_file_reader.hpp"
#include "scripting_test_utilities.hpp"
#include <climits>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace rive;

namespace
{
std::vector<uint8_t> compileLuau(const char* src)
{
    size_t size = 0;
    char* bytecode = luau_compile(src, strlen(src), nullptr, &size);
    REQUIRE(bytecode != nullptr);
    std::vector<uint8_t> out((uint8_t*)bytecode, (uint8_t*)bytecode + size);
    free(bytecode);
    return out;
}

// Inline source registered under a flat (possibly mangled) module name.
class ScopeTestModule : public ModuleDetails
{
public:
    ScopeTestModule(std::string name, const char* src, bool isModule = true) :
        m_name(std::move(name)),
        m_bytecode(compileLuau(src)),
        m_isModule(isModule)
    {}
    std::string moduleName() override { return m_name; }
    Span<uint8_t> moduleBytecode() override
    {
        return Span<uint8_t>(m_bytecode.data(), m_bytecode.size());
    }
    bool isProtocolScript() override { return !m_isModule; }
    bool verified() const override { return true; }

private:
    std::string m_name;
    std::vector<uint8_t> m_bytecode;
    bool m_isModule;
};

class ScopeTestContext : public ScriptingContext
{
public:
    ScopeTestContext(Factory* factory) : ScriptingContext(factory) {}
    void printBeginLine(lua_State*) override {}
    void print(Span<const char>) override {}
    void printEndLine() override {}
    void printError(lua_State* state) override
    {
        const char* error = lua_tostring(state, -1);
        if (error != nullptr)
        {
            errors.push_back(error);
        }
    }
    int pCall(lua_State* state, int nargs, int nresults) override
    {
        return lua_pcall(state, nargs, nresults, 0);
    }
    std::vector<std::string> errors;
};

// Reads the integer field `field` of the module cached under `key` in _MODULES.
// Returns INT_MIN when the module or field is absent.
int cachedIntField(lua_State* L, const std::string& key, const char* field)
{
    luaL_findtable(L, LUA_REGISTRYINDEX, "_MODULES", 1);
    lua_getfield(L, -1, key.c_str());
    int result = INT_MIN;
    if (lua_istable(L, -1))
    {
        lua_getfield(L, -1, field);
        if (lua_isnumber(L, -1))
        {
            result = (int)lua_tointeger(L, -1);
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 2); // module result + _MODULES table
    return result;
}
} // namespace

TEST_CASE("statically linked flat names round trip", "[scripting][scope]")
{
    // The S contract: the editor rewrites requires to mangled flat names and
    // registers modules under them at root scope. No pins, no scope props;
    // diamonds coexist because every version has a distinct name.
    SerializingFactory factory;
    auto ctxOwned = std::make_unique<ScopeTestContext>(&factory);
    ScopeTestContext* ctx = ctxOwned.get();
    auto vm = make_rcp<ScriptingVM>(std::move(ctxOwned));
    lua_State* L = vm->state();

    ScopeTestModule meshV1("draco@1/mesh", "return { v = 1 }");
    ScopeTestModule meshV2("draco@2/mesh", "return { v = 2 }");
    ScopeTestModule uses("B@1/uses",
                         "return { v = require('draco@1/mesh').v }");
    ScopeTestModule app("app",
                        "return { v = require('draco@2/mesh').v + "
                        "require('B@1/uses').v }");

    ctx->addModule(&meshV1);
    ctx->addModule(&meshV2);
    ctx->addModule(&uses);
    ctx->addModule(&app);
    ctx->performRegistration(L);

    CHECK(cachedIntField(L, "draco@1/mesh", "v") == 1);
    CHECK(cachedIntField(L, "draco@2/mesh", "v") == 2);
    CHECK(cachedIntField(L, "B@1/uses", "v") == 1);
    CHECK(cachedIntField(L, "app", "v") == 3);
    CHECK(ctx->errors.empty());
}

TEST_CASE("bare require never reaches a mangled library module",
          "[scripting][scope]")
{
    SerializingFactory factory;
    auto ctxOwned = std::make_unique<ScopeTestContext>(&factory);
    ScopeTestContext* ctx = ctxOwned.get();
    auto vm = make_rcp<ScriptingVM>(std::move(ctxOwned));
    lua_State* L = vm->state();

    ScopeTestModule mesh("draco@1/mesh", "return { v = 1 }");
    ScopeTestModule app("app", "return require('mesh')");
    ctx->addModule(&mesh);
    ctx->addModule(&app);
    ctx->performRegistration(L);

    CHECK(cachedIntField(L, "app", "v") == INT_MIN);
    REQUIRE(!ctx->errors.empty());
}

TEST_CASE("mangled module errors attribute to the library in traces",
          "[scripting][scope]")
{
    SerializingFactory factory;
    auto ctxOwned = std::make_unique<ScopeTestContext>(&factory);
    ScopeTestContext* ctx = ctxOwned.get();
    auto vm = make_rcp<ScriptingVM>(std::move(ctxOwned));
    lua_State* L = vm->state();

    ScopeTestModule bad("draco@1/boom", "error('boom')");
    ctx->addModule(&bad);
    ctx->performRegistration(L);

    REQUIRE(!ctx->errors.empty());
    bool sawMangled = false;
    for (const auto& error : ctx->errors)
    {
        if (error.find("draco@1/boom:") != std::string::npos)
        {
            sawMangled = true;
        }
    }
    CHECK(sawMangled);
}

TEST_CASE("scoped asset reference matching", "[scripting][scope]")
{
    // Host caller (no Lua frame): bare names match host assets only.
    ScriptingContext::ScopedAssetReference bare(nullptr, "lit");
    CHECK(bare.match("lit", "lit") == 1);
    CHECK(bare.match("shaders/lit", "lit") == 1);
    CHECK(bare.match("draco@1531/lit", "lit") == 0);
    CHECK(bare.match("draco@1531/other", "other") == 0);

    // lib: references match any version of the label's library.
    ScriptingContext::ScopedAssetReference lib(nullptr, "lib:draco/lit");
    CHECK(lib.match("draco@1531/lit", "lit") == 1);
    CHECK(lib.match("draco#9@2/lit", "lit") == 1);
    CHECK(lib.match("draco@1531/other", "other") == 0);
    CHECK(lib.match("other@3/lit", "lit") == 0);
    CHECK(lib.match("lit", "lit") == 0);
    CHECK(lib.match("dracoish@3/lit", "lit") == 0);
}

TEST_CASE("exported file resolves statically linked library requires",
          "[scripting][scope][libraries]")
{
    // Exported by the editor: draco imported as a library, a host module
    // scope_probe that lib:-requires it, bare-requires it under pcall, and
    // records the outcomes. The require was rewritten to draco@<ver>/draco
    // at export; the file carries no edges and no scope props, everything
    // resolves through the flat module cache.
    auto file = ReadRiveFile("assets/scope_probe.riv");
    REQUIRE(file->scriptingVM() != nullptr);
    lua_State* L = file->scriptingVM()->state();

    // Qualified require resolved and returned the real module.
    CHECK(cachedIntField(L, "scope_probe", "lib") == 1);
    CHECK(cachedIntField(L, "scope_probe", "hasDecode") == 1);
    // Same request twice hits the scoped cache, not a re-execution.
    CHECK(cachedIntField(L, "scope_probe", "cached") == 1);
    // A bare require from the host never leaks into the library scope.
    auto bc = compileLuau("local ok = pcall(require, 'draco')\n"
                          "return { ok = ok and 1 or 0 }");
    REQUIRE(ScriptingVM::registerModule(L,
                                        "leak_probe",
                                        Span<uint8_t>(bc.data(), bc.size()),
                                        "leak_probe"));
    CHECK(cachedIntField(L, "leak_probe", "ok") == 0);
}

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

// Inline source with an explicit scope so tests can register one name under
// different library versions.
class ScopeTestModule : public ModuleDetails
{
public:
    ScopeTestModule(std::string name,
                    ScopeKey scope,
                    const char* src,
                    bool isModule = true) :
        m_name(std::move(name)),
        m_scope(scope),
        m_bytecode(compileLuau(src)),
        m_isModule(isModule)
    {}
    std::string moduleName() override { return m_name; }
    ScopeKey scope() override { return m_scope; }
    Span<uint8_t> moduleBytecode() override
    {
        return Span<uint8_t>(m_bytecode.data(), m_bytecode.size());
    }
    bool isProtocolScript() override { return !m_isModule; }
    bool verified() const override { return true; }

private:
    std::string m_name;
    ScopeKey m_scope;
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

TEST_CASE("two versions of one module coexist", "[scripting][scope]")
{
    SerializingFactory factory;
    auto ctxOwned = std::make_unique<ScopeTestContext>(&factory);
    ScopeTestContext* ctx = ctxOwned.get();
    auto vm = make_rcp<ScriptingVM>(std::move(ctxOwned));
    lua_State* L = vm->state();

    ScopeKey v1{1, 1};
    ScopeKey v2{1, 2};
    // Same module name, two library versions, different bodies.
    ScopeTestModule fooV1("utils/math", v1, "return { v = 1 }");
    ScopeTestModule fooV2("utils/math", v2, "return { v = 2 }");
    // Same caller name in each version; each bare-requires "utils/math".
    ScopeTestModule callerA("caller", v1, "return require('utils/math')");
    ScopeTestModule callerB("caller", v2, "return require('utils/math')");

    ctx->addModule(&fooV1);
    ctx->addModule(&fooV2);
    ctx->addModule(&callerA);
    ctx->addModule(&callerB);
    ctx->performRegistration(L);

    // Both versions coexist under distinct scoped keys.
    CHECK(cachedIntField(L,
                         ScriptingContext::scopedKey("utils/math", v1),
                         "v") == 1);
    CHECK(cachedIntField(L,
                         ScriptingContext::scopedKey("utils/math", v2),
                         "v") == 2);
    // Each caller's bare require bound its own version (relative scope).
    CHECK(cachedIntField(L, ScriptingContext::scopedKey("caller", v1), "v") ==
          1);
    CHECK(cachedIntField(L, ScriptingContext::scopedKey("caller", v2), "v") ==
          2);
    CHECK(ctx->errors.empty());
}

TEST_CASE("qualified library require resolves via pins", "[scripting][scope]")
{
    SerializingFactory factory;
    auto ctxOwned = std::make_unique<ScopeTestContext>(&factory);
    ScopeTestContext* ctx = ctxOwned.get();
    auto vm = make_rcp<ScriptingVM>(std::move(ctxOwned));
    lua_State* L = vm->state();

    ScopeKey geo{7, 3};
    ScopeTestModule libMesh("mesh", geo, "return { v = 42 }");
    // Host has its own 'mesh' at root scope; must not collide with the
    // library's.
    ScopeTestModule hostMesh("mesh", ScopeKey{}, "return { v = 7 }");
    ScopeTestModule app("app",
                        ScopeKey{},
                        "local d = require('lib:geo/mesh')\n"
                        "local h = require('mesh')\n"
                        "return { v = d.v, host = h.v }");

    ctx->addImport(ScopeKey{}, "geo", geo); // host pins geo -> (7,3)
    ctx->addModule(&libMesh);
    ctx->addModule(&hostMesh);
    ctx->addModule(&app);
    ctx->performRegistration(L);

    // Qualified require reached the library's mesh; bare require stayed at host
    // root.
    CHECK(cachedIntField(L, "app", "v") == 42);
    CHECK(cachedIntField(L, "app", "host") == 7);
    CHECK(ctx->errors.empty());
}

TEST_CASE("per caller pins diverge on the same label", "[scripting][scope]")
{
    // The diamond: host pins A to v2, library B pins A to v1. The label is
    // per caller, both resolve to their own version.
    SerializingFactory factory;
    auto ctxOwned = std::make_unique<ScopeTestContext>(&factory);
    ScopeTestContext* ctx = ctxOwned.get();
    auto vm = make_rcp<ScriptingVM>(std::move(ctxOwned));
    lua_State* L = vm->state();

    ScopeKey aV1{7, 1};
    ScopeKey aV2{7, 2};
    ScopeKey b{8, 1};
    ScopeTestModule meshV1("mesh", aV1, "return { v = 1 }");
    ScopeTestModule meshV2("mesh", aV2, "return { v = 2 }");
    ScopeTestModule bUses("uses", b, "return require('lib:A/mesh')");
    ScopeTestModule app("app",
                        ScopeKey{},
                        "return { v = require('lib:A/mesh').v }");

    ctx->addImport(ScopeKey{}, "A", aV2);
    ctx->addImport(b, "A", aV1);
    ctx->addModule(&meshV1);
    ctx->addModule(&meshV2);
    ctx->addModule(&bUses);
    ctx->addModule(&app);
    ctx->performRegistration(L);

    CHECK(cachedIntField(L, "app", "v") == 2);
    CHECK(cachedIntField(L, ScriptingContext::scopedKey("uses", b), "v") == 1);
    CHECK(ctx->errors.empty());
}

TEST_CASE("unresolved library require errors, never rebinds to caller scope",
          "[scripting][scope]")
{
    // An unpinned library require must fail, not rebind to a same named
    // module in the caller's scope.
    SerializingFactory factory;
    auto ctxOwned = std::make_unique<ScopeTestContext>(&factory);
    ScopeTestContext* ctx = ctxOwned.get();
    auto vm = make_rcp<ScriptingVM>(std::move(ctxOwned));
    lua_State* L = vm->state();

    // Host has its own 'mesh'; no geo pin is ever registered.
    ScopeTestModule hostMesh("mesh", ScopeKey{}, "return { v = 7 }");
    ScopeTestModule app("app",
                        ScopeKey{},
                        "return { v = require('lib:geo/mesh').v }");
    ctx->addModule(&hostMesh);
    ctx->addModule(&app);
    ctx->performRegistration(L);

    // app must NOT have resolved (would be cached if it had); it errored.
    CHECK(cachedIntField(L, "app", "v") == INT_MIN);
    REQUIRE(!ctx->errors.empty());
    bool reportedMissing = false;
    for (const auto& error : ctx->errors)
    {
        if (error.find("lib:geo/mesh") != std::string::npos)
        {
            reportedMissing = true;
        }
    }
    CHECK(reportedMissing);
}

TEST_CASE("ScriptAsset scope reads serialized FileAsset scope props",
          "[scripting][scope]")
{
    ScriptAsset asset;
    CHECK(asset.scope().isRoot()); // default (0,0) = host/root

    asset.scopeLibraryId(7);
    asset.scopeLibraryVersionId(3);
    CHECK(asset.scope().libraryId == 7);
    CHECK(asset.scope().libraryVersionId == 3);
    CHECK(!asset.scope().isRoot());
}

TEST_CASE("scoped module errors use readable names", "[scripting][scope]")
{
    SerializingFactory factory;
    auto ctxOwned = std::make_unique<ScopeTestContext>(&factory);
    ScopeTestContext* ctx = ctxOwned.get();
    auto vm = make_rcp<ScriptingVM>(std::move(ctxOwned));
    lua_State* L = vm->state();

    ScopeKey v1{1, 1};
    ScopeTestModule bad("thing", v1, "error('boom')");
    ctx->addModule(&bad);
    ctx->performRegistration(L);

    REQUIRE(!ctx->errors.empty());
    bool sawReadable = false;
    for (const auto& error : ctx->errors)
    {
        if (error.find("1-1/thing") != std::string::npos)
        {
            sawReadable = true;
        }
        // The internal scoped-key delimiter must never leak into traces.
        CHECK(error.find('\x1f') == std::string::npos);
    }
    CHECK(sawReadable);
}

TEST_CASE("exported file resolves lib: requires through import edges",
          "[scripting][scope][libraries]")
{
    // Exported by the editor: draco imported as a library, a host module
    // scope_probe that lib:-requires it, bare-requires it under pcall, and
    // records the outcomes. The whole scope pipeline in one file: edge and
    // scope props deserialize, registerScripts seeds the pins, scoped cache
    // keys isolate the library, the edge resolves the qualified require.
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

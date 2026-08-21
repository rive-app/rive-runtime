#ifdef RIVE_WASM_MODULE
// The script module's ScriptingContext: enough for the compiled-in binding
// files, whose only host contact is factory() and the print/pcall policy.
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/wasm/module_render.hpp"

#include "lualib.h"

#include <stdio.h>

using namespace rive;

int luaopen_rive_path(lua_State* L);
int luaopen_rive_gradient(lua_State* L);
int luaopen_rive_paint(lua_State* L);
int luaopen_rive_renderer(lua_State* L);
int luaopen_rive_image(lua_State* L);
int luaopen_rive_mesh(lua_State* L);
int luaopen_rive_blob(lua_State* L);

namespace
{
class ModuleScriptingContext : public ScriptingContext
{
public:
    ModuleScriptingContext() : ScriptingContext(wasmModuleFactory()) {}

    void printError(lua_State* state) override
    {
        const char* message = lua_tostring(state, -1);
        fprintf(stderr, "script error: %s\n", message ? message : "?");
    }
    void printBeginLine(lua_State*) override {}
    void print(Span<const char> data) override
    {
        fwrite(data.data(), 1, data.size(), stdout);
    }
    void printEndLine() override { fputc('\n', stdout); }
    int pCall(lua_State* state, int nargs, int nresults) override
    {
        int status = lua_pcall(state, nargs, nresults, 0);
        if (status != LUA_OK)
        {
            printError(state);
        }
        return status;
    }
};
} // namespace

namespace rive
{
ScriptingContext* wasmModuleContext()
{
    static ModuleScriptingContext context;
    return &context;
}

// Path caching keys on the artboard frame id, which lives host side. A
// constant here makes every dirty rebuild count as same-frame, which
// allocates a fresh render path instead of rewinding — conservative and
// never stale.
uint64_t Artboard::sm_frameId = 0;

// The module has no async work pool.
void ScriptingContext::shutdownAsync() {}

// Host side this is a helper in the Luau backend TU; the blob only needs
// the ref pushed.
int rive_lua_pushRef(lua_State* L, int ref)
{
    lua_getref(L, ref);
    return 1;
}
} // namespace rive

// Rendering surface for the module: the untouched binding files over the
// handle-backed proxies.
int luaopen_rive_renderer_module(lua_State* L)
{
    return luaopen_rive_path(L) + luaopen_rive_gradient(L) +
           luaopen_rive_paint(L) + luaopen_rive_renderer(L) +
           luaopen_rive_image(L) + luaopen_rive_mesh(L) + luaopen_rive_blob(L);
}

#endif

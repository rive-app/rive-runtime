#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/span.hpp"

using namespace rive;
static int luaB_print(lua_State* L)
{
    int n = lua_gettop(L); // number of arguments
    if (n == 0)
    {
        return 0;
    }

    ScriptingContext* context =
        static_cast<ScriptingContext*>(lua_getthreaddata(L));

    context->printBeginLine(L);
    for (int i = 1; i <= n; i++)
    {
        size_t l;
        const char* s =
            luaL_tolstring(L,
                           i,
                           &l); // convert to string using __tostring et al
        context->print(Span<const char>(s, l));
        lua_pop(L, 1); // pop result
    }
    context->printEndLine();
    return 0;
}

static const luaL_Reg base_funcs[] = {
    {"print", luaB_print},
    {NULL, NULL},
};

int luaopen_rive_base(lua_State* L)
{
    // open lib into global table
    luaL_register(L, "_G", base_funcs);

    return 1;
}

#endif
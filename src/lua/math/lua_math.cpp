#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"

int luaopen_rive_vector(lua_State* L);
int luaopen_rive_mat2d(lua_State* L);
int luaopen_rive_color(lua_State* L);

static const lua_CFunction mathTypes[] = {luaopen_rive_vector,
                                          luaopen_rive_mat2d,
                                          luaopen_rive_color};

int luaopen_rive_math(lua_State* L)
{
    int added = 0;
    for (auto type : mathTypes)
    {
        added += type(L);
    }
    return added;
}

#endif

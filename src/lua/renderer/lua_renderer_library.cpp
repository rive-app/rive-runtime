#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"

int luaopen_rive_path(lua_State* L);
int luaopen_rive_gradient(lua_State* L);
int luaopen_rive_mesh(lua_State* L);
int luaopen_rive_image(lua_State* L);
int luaopen_rive_paint(lua_State* L);
int luaopen_rive_renderer(lua_State* L);

static const lua_CFunction rendererTypes[] = {luaopen_rive_path,
                                              luaopen_rive_gradient,
                                              luaopen_rive_mesh,
                                              luaopen_rive_image,
                                              luaopen_rive_paint,
                                              luaopen_rive_renderer};

int luaopen_rive_renderer_library(lua_State* L)
{
    int added = 0;
    for (auto type : rendererTypes)
    {
        added += type(L);
    }
    return added;
}

#endif

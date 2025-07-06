#ifdef WITH_RIVE_SCRIPTING
#include "lua.h"
#include "lualib.h"
#include "rive/lua/rive_lua_libs.hpp"

using namespace rive;

static int image_index(lua_State* L)
{
    auto image = lua_torive<ScriptedImage>(L, 1);

    int atom;
    const char* name = lua_tostringatom(L, 2, &atom);
    if (!name)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
    }
    switch (atom)
    {
        case (int)LuaAtoms::width:
            lua_pushnumber(L, image->image ? image->image->width() : 0);
            break;
        case (int)LuaAtoms::height:
            lua_pushnumber(L, image->image ? image->image->height() : 0);
            break;
    }
    luaL_error(L,
               "'%s' is not a valid index of %s",
               name,
               ScriptedImage::luaName);
    return 0;
}

static int imagesampler_construct(lua_State* L)
{
    int wrapXAtom;
    const char* wrapXName = lua_tostringatom(L, 1, &wrapXAtom);
    if (!wrapXName)
    {
        luaL_typeerrorL(L, 1, lua_typename(L, LUA_TSTRING));
    }
    int wrapYAtom;
    const char* wrapYName = lua_tostringatom(L, 2, &wrapYAtom);
    if (!wrapYName)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
    }
    int imageFilterAtom;
    const char* imageFilterName = lua_tostringatom(L, 3, &imageFilterAtom);
    if (!imageFilterName)
    {
        luaL_typeerrorL(L, 3, lua_typename(L, LUA_TSTRING));
    }
    ImageWrap wrapX = ImageWrap::clamp;
    ImageWrap wrapY = ImageWrap::clamp;
    ImageFilter filter = ImageFilter::trilinear;
    switch (wrapXAtom)
    {
        case (int)LuaAtoms::clamp:
            break;
        case (int)LuaAtoms::repeat:
            wrapX = ImageWrap::repeat;
            break;
        case (int)LuaAtoms::mirror:
            wrapX = ImageWrap::mirror;
            break;
        default:
            luaL_error(L, "'%s' is not a valid ImageWrap", wrapXName);
            break;
    }
    switch (wrapYAtom)
    {
        case (int)LuaAtoms::clamp:
            break;
        case (int)LuaAtoms::repeat:
            wrapY = ImageWrap::repeat;
            break;
        case (int)LuaAtoms::mirror:
            wrapY = ImageWrap::mirror;
            break;
        default:
            luaL_error(L, "'%s' is not a valid ImageWrap", wrapYName);
            break;
    }

    switch (imageFilterAtom)
    {
        case (int)LuaAtoms::trilinear:
            break;
        case (int)LuaAtoms::nearest:
            filter = ImageFilter::nearest;
            break;
        default:
            luaL_error(L, "'%s' is not a valid ImageFilter", imageFilterName);
            break;
    }

    lua_newrive<ScriptedImageSampler>(L, wrapX, wrapY, filter);
    return 1;
}

static const luaL_Reg imageStaticMethods[] = {{nullptr, nullptr}};

int luaopen_rive_image(lua_State* L)
{
    luaL_register(L, ScriptedImage::luaName, imageStaticMethods);
    lua_register_rive<ScriptedImage>(L);

    lua_pushcfunction(L, image_index, nullptr);
    lua_setfield(L, -2, "__index");
    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_pushcfunction(L, imagesampler_construct, ScriptedImageSampler::luaName);
    lua_setfield(L, LUA_GLOBALSINDEX, ScriptedImageSampler::luaName);
    lua_register_rive<ScriptedImageSampler>(L);

    return 2;
}

#endif

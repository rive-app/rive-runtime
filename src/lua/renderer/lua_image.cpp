#ifdef WITH_RIVE_SCRIPTING
#include "lua.h"
#include "lualib.h"
#include "rive/lua/rive_lua_libs.hpp"

using namespace rive;

// Defined in lua_gpu.cpp (compiled as ObjC++ on Apple, has ore includes).
#ifdef RIVE_ORE
extern int riveImageViewImpl(lua_State* L);
#endif

// Out-of-line destructor. The rcp<ore::TextureView> member (when
// RIVE_CANVAS && RIVE_ORE) prevents implicit ~ScriptedImage() in TUs
// that don't include the full ore headers. When ore is not enabled the
// default-generated body is fine.
#if !(defined(RIVE_CANVAS) && defined(RIVE_ORE))
ScriptedImage::~ScriptedImage() = default;
ScriptedImage* ScriptedImage::luaNew(lua_State* L)
{
    return lua_newrive<ScriptedImage>(L);
}
#endif

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
            return 1;
        case (int)LuaAtoms::height:
            lua_pushnumber(L, image->image ? image->image->height() : 0);
            return 1;
#ifdef RIVE_ORE
        case (int)LuaAtoms::view:
            lua_pushcfunction(L, riveImageViewImpl, "Image.view");
            return 1;
#endif
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
    ImageFilter filter = ImageFilter::bilinear;
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
        case (int)LuaAtoms::bilinear:
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

int luaopen_rive_image(lua_State* L)
{
    lua_register_rive<ScriptedImage>(L);

    lua_pushcfunction(L, image_index, nullptr);
    lua_setfield(L, -2, "__index");
    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_pushcfunction(L, imagesampler_construct, ScriptedImageSampler::luaName);
    lua_setfield(L, LUA_GLOBALSINDEX, ScriptedImageSampler::luaName);
    lua_register_rive<ScriptedImageSampler>(L);

    return 0;
}

#endif

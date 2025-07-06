#ifdef WITH_RIVE_SCRIPTING
#include "lua.h"
#include "rive/lua/rive_lua_libs.hpp"
#include "lualib.h"
#include "rive/shapes/paint/color.hpp"

using namespace rive;

static int color_construct(lua_State* L)
{
    int args = lua_gettop(L) - 1;

    uint32_t r = luaL_checkunsigned(L, 2);
    uint32_t g = luaL_checknumber(L, 3);
    uint32_t b = luaL_checknumber(L, 4);
    uint32_t a = args == 4 ? luaL_checknumber(L, 5) : 255;
    lua_pushunsigned(L, colorARGB(a, r, g, b));

    return 1;
}

static int color_red(lua_State* L)
{
    uint32_t color = luaL_checkunsigned(L, 1);
    int isnum;
    uint32_t red = lua_tounsignedx(L, 2, &isnum);
    if (isnum == 1)
    {
        lua_pushunsigned(L,
                         colorARGB(colorAlpha(color),
                                   red,
                                   colorGreen(color),
                                   colorBlue(color)));
    }
    else
    {
        lua_pushunsigned(L, colorRed(color));
    }
    return 1;
}

static int color_green(lua_State* L)
{
    uint32_t color = luaL_checkunsigned(L, 1);
    int isnum;
    uint32_t green = lua_tounsignedx(L, 2, &isnum);
    if (isnum == 1)
    {
        lua_pushunsigned(L,
                         colorARGB(colorAlpha(color),
                                   colorRed(color),
                                   green,
                                   colorBlue(color)));
    }
    else
    {
        lua_pushunsigned(L, colorGreen(color));
    }
    return 1;
}

static int color_blue(lua_State* L)
{
    uint32_t color = luaL_checkunsigned(L, 1);
    int isnum;
    uint32_t blue = lua_tounsignedx(L, 2, &isnum);
    if (isnum == 1)
    {
        lua_pushunsigned(L,
                         colorARGB(colorAlpha(color),
                                   colorRed(color),
                                   colorGreen(color),
                                   blue));
    }
    else
    {
        lua_pushunsigned(L, colorBlue(color));
    }
    return 1;
}

static int color_alpha(lua_State* L)
{
    uint32_t color = luaL_checkunsigned(L, 1);
    int isnum;
    uint32_t alpha = lua_tounsignedx(L, 2, &isnum);
    if (isnum == 1)
    {
        lua_pushunsigned(L,
                         colorARGB(alpha,
                                   colorRed(color),
                                   colorGreen(color),
                                   colorBlue(color)));
    }
    else
    {
        lua_pushunsigned(L, colorAlpha(color));
    }
    return 1;
}

static int color_opacity(lua_State* L)
{
    uint32_t color = luaL_checkunsigned(L, 1);
    int isnum;
    float opacity = float(lua_tonumberx(L, 2, &isnum));
    if (isnum == 1)
    {
        lua_pushunsigned(L,
                         colorARGB(opacityToAlpha(opacity),
                                   colorRed(color),
                                   colorGreen(color),
                                   colorBlue(color)));
    }
    else
    {
        lua_pushnumber(L, colorOpacity(color));
    }
    return 1;
}

static int color_lerp(lua_State* L)
{
    uint32_t from = luaL_checkunsigned(L, 1);
    uint32_t to = luaL_checkunsigned(L, 2);
    float f = float(luaL_checknumber(L, 3));
    lua_pushnumber(L, colorLerp(from, to, f));
    return 1;
}

static const luaL_Reg colorStaticMethods[] = {{"red", color_red},
                                              {"green", color_green},
                                              {"blue", color_blue},
                                              {"alpha", color_alpha},
                                              {"opacity", color_opacity},
                                              {"lerp", color_lerp},
                                              {nullptr, nullptr}};

int luaopen_rive_color(lua_State* L)
{
    luaL_register(L, "Color", colorStaticMethods);
    lua_createtable(L, 0, 1);
    lua_pushcfunction(L, color_construct, nullptr);
    lua_setfield(L, -2, "__call");
    lua_setmetatable(L, -2);

    return 1;
}

#endif

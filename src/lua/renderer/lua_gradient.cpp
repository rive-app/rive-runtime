#ifdef WITH_RIVE_SCRIPTING
#include "lua.h"
#include "lualib.h"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/factory.hpp"

using namespace rive;

static void fill_stops(lua_State* L,
                       std::vector<float>& stops,
                       std::vector<ColorInt>& colors)
{
    luaL_checktype(L, 3, LUA_TTABLE);
    int index = 1;
    while (true)
    {
        if (lua_rawgeti(L, 3, index++) != LUA_TTABLE)
        {
            lua_pop(L, 1);
            break;
        }
        lua_rawgetfield(L, -1, "position");
        float position = float(luaL_checknumber(L, -1));
        lua_pop(L, 1);
        lua_rawgetfield(L, -1, "color");
        uint32_t color = luaL_checkunsigned(L, -1);

        stops.push_back(position);
        colors.push_back(color);
        lua_pop(L, 1);
    }
}
static int gradient_linear(lua_State* L)
{
    auto from = lua_checkvec2d(L, 1);
    auto to = lua_checkvec2d(L, 2);

    std::vector<float> stops;
    std::vector<ColorInt> colors;
    fill_stops(L, stops, colors);

    ScriptedGradient* gradient = lua_newrive<ScriptedGradient>(L);
    ScriptingContext* context =
        static_cast<ScriptingContext*>(lua_getthreaddata(L));

    gradient->shader = context->factory->makeLinearGradient(from->x,
                                                            from->y,
                                                            to->x,
                                                            to->y,
                                                            colors.data(),
                                                            stops.data(),
                                                            stops.size());
    return 1;
}

static int gradient_radial(lua_State* L)
{
    auto from = lua_checkvec2d(L, 1);
    auto radius = luaL_checknumber(L, 2);
    std::vector<float> stops;
    std::vector<ColorInt> colors;
    fill_stops(L, stops, colors);

    ScriptedGradient* gradient = lua_newrive<ScriptedGradient>(L);
    ScriptingContext* context =
        static_cast<ScriptingContext*>(lua_getthreaddata(L));

    gradient->shader = context->factory->makeRadialGradient(from->x,
                                                            from->y,
                                                            radius,
                                                            colors.data(),
                                                            stops.data(),
                                                            stops.size());
    return 1;
}

static const luaL_Reg gradientStaticMethods[] = {{"linear", gradient_linear},
                                                 {"radial", gradient_radial},
                                                 {nullptr, nullptr}};

int luaopen_rive_gradient(lua_State* L)
{
    luaL_register(L, ScriptedGradient::luaName, gradientStaticMethods);
    lua_register_rive<ScriptedGradient>(L);

    return 1;
}

#endif

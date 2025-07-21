#ifdef WITH_RIVE_SCRIPTING
#include "lua.h"
#include "rive/renderer.hpp"
#include "rive/rive_types.hpp"
#include "rive/shapes/paint/blend_mode.hpp"
#include "lualib.h"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/factory.hpp"

using namespace rive;

static void readStyle(lua_State* L, ScriptedPaint* paint, int index)
{
    int atom;
    const char* styleName = lua_tostringatom(L, index, &atom);
    if (!styleName)
    {
        luaL_typeerrorL(L, index, lua_typename(L, LUA_TSTRING));
    }
    switch (atom)
    {
        case (int)LuaAtoms::stroke:
            paint->style(RenderPaintStyle::stroke);
            break;
        case (int)LuaAtoms::fill:
            paint->style(RenderPaintStyle::fill);
            break;
        default:
            luaL_error(L, "'%s' is not a valid PaintStyle", styleName);
            break;
    }
}

static void readJoin(lua_State* L, ScriptedPaint* paint, int index)
{
    int atom;
    const char* joinName = lua_tostringatom(L, index, &atom);
    if (!joinName)
    {
        luaL_typeerrorL(L, index, lua_typename(L, LUA_TSTRING));
    }
    switch (atom)
    {
        case (int)LuaAtoms::miter:
            paint->join(StrokeJoin::miter);
            break;
        case (int)LuaAtoms::round:
            paint->join(StrokeJoin::round);
            break;
        case (int)LuaAtoms::bevel:
            paint->join(StrokeJoin::bevel);
            break;
        default:
            luaL_error(L, "'%s' is not a valid StrokeJoin", joinName);
            break;
    }
}

static void readCap(lua_State* L, ScriptedPaint* paint, int index)
{
    int atom;
    const char* capName = lua_tostringatom(L, index, &atom);
    if (!capName)
    {
        luaL_typeerrorL(L, index, lua_typename(L, LUA_TSTRING));
    }
    switch (atom)
    {
        case (int)LuaAtoms::butt:
            paint->cap(StrokeCap::butt);
            break;
        case (int)LuaAtoms::round:
            paint->cap(StrokeCap::round);
            break;
        case (int)LuaAtoms::square:
            paint->cap(StrokeCap::square);
            break;
        default:
            luaL_error(L, "'%s' is not a valid StrokeCap", capName);
            break;
    }
}

BlendMode rive::lua_toblendmode(lua_State* L, int idx)
{
    int atom;
    const char* blendName = lua_tostringatom(L, idx, &atom);
    if (!blendName)
    {
        luaL_typeerrorL(L, idx, lua_typename(L, LUA_TSTRING));
    }
    switch (atom)
    {
        case (int)LuaAtoms::srcOver:
            return BlendMode::srcOver;

        case (int)LuaAtoms::screen:
            return BlendMode::screen;

        case (int)LuaAtoms::overlay:
            return BlendMode::overlay;

        case (int)LuaAtoms::darken:
            return BlendMode::darken;

        case (int)LuaAtoms::lighten:
            return BlendMode::lighten;

        case (int)LuaAtoms::colorDodge:
            return BlendMode::colorDodge;

        case (int)LuaAtoms::colorBurn:
            return BlendMode::colorBurn;

        case (int)LuaAtoms::hardLight:
            return BlendMode::hardLight;

        case (int)LuaAtoms::softLight:
            return BlendMode::softLight;

        case (int)LuaAtoms::difference:
            return BlendMode::difference;

        case (int)LuaAtoms::exclusion:
            return BlendMode::exclusion;

        case (int)LuaAtoms::multiply:
            return BlendMode::multiply;

        case (int)LuaAtoms::hue:
            return BlendMode::hue;

        case (int)LuaAtoms::saturation:
            return BlendMode::saturation;

        case (int)LuaAtoms::color:
            return BlendMode::color;

        case (int)LuaAtoms::luminosity:
            return BlendMode::luminosity;

        default:
            luaL_error(L, "'%s' is not a valid BlendMode", blendName);
            return BlendMode::srcOver;
    }
}

static void readBlendMode(lua_State* L, ScriptedPaint* paint, int index)
{
    auto blendMode = lua_toblendmode(L, index);
    paint->blendMode(blendMode);
}

ScriptedPaint::ScriptedPaint(Factory* factory) :
    renderPaint(factory->makeRenderPaint())
{}

ScriptedPaint::ScriptedPaint(Factory* factory, const ScriptedPaint& source) :
    ScriptedPaint(factory)
{
    style(source.m_style);
    color(source.m_color);
    thickness(source.m_thickness);
    join(source.m_join);
    cap(source.m_cap);
    feather(source.m_feather);
    blendMode(source.m_blendMode);
    gradient(source.m_gradient);
}

static bool paint_set_value(lua_State* L,
                            ScriptedPaint* renderPaint,
                            int keyAtom,
                            int valueIndex)
{
    switch (keyAtom)
    {
        case (int)LuaAtoms::style:
            readStyle(L, renderPaint, valueIndex);
            return true;
        case (int)LuaAtoms::join:
            readJoin(L, renderPaint, valueIndex);
            return true;
        case (int)LuaAtoms::cap:
            readCap(L, renderPaint, valueIndex);
            return true;
        case (int)LuaAtoms::thickness:
            renderPaint->thickness(float(luaL_checknumber(L, valueIndex)));
            return true;
        case (int)LuaAtoms::blendMode:
            readBlendMode(L, renderPaint, valueIndex);
            return true;
        case (int)LuaAtoms::feather:
            renderPaint->feather(float(luaL_checknumber(L, valueIndex)));
            return true;
        case (int)LuaAtoms::gradient:
        {
            ScriptedGradient* gradient =
                lua_torive<ScriptedGradient>(L, valueIndex, true);
            if (gradient != nullptr)
            {
                renderPaint->gradient(gradient->shader);
            }
            else
            {
                renderPaint->gradient(nullptr);
            }
            return true;
        }
        case (int)LuaAtoms::color:
            renderPaint->color(luaL_checkunsigned(L, valueIndex));
            return true;
        default:
            return false;
    }
}

static void setPropertiesFromDefinitionTable(lua_State* L,
                                             ScriptedPaint* scriptedPaint,
                                             int tableIndex)
{
    luaL_checktype(L, tableIndex, LUA_TTABLE);

    // iterate table
    // Push another reference to the table on top of the stack (so we know
    // where it is, and this function can work for negative, positive and
    // pseudo indices
    lua_pushvalue(L, tableIndex);
    // stack now contains: -1 => table
    lua_pushnil(L);
    // stack now contains: -1 => nil; -2 => table
    while (lua_next(L, -2))
    {
        // stack now contains: -1 => value; -2 => key; -3 => table
        // copy the key so that lua_tostring does not modify the original
        // lua_pushvalue(L, -2);
        // stack now contains: -1 => key; -2 => value; -3 => key; -4 =>
        // table
        int atom;
        const char* key = lua_tostringatom(L, -2, &atom);
        if (key)
        {
            paint_set_value(L, scriptedPaint, atom, -1);
        }
        lua_pop(L, 1);
        // stack now contains: -1 => key; -2 => table
    }
    // stack now contains: -1 => table (when lua_next returns 0 it pops the
    // key but does not push anything.) Pop table
    lua_pop(L, 1);
}

static int paint_new(lua_State* L)
{
    ScriptingContext* context =
        static_cast<ScriptingContext*>(lua_getthreaddata(L));
    lua_newrive<ScriptedPaint>(L, context->factory);

    return 1;
}

static int paint_with(lua_State* L)
{
    ScriptingContext* context =
        static_cast<ScriptingContext*>(lua_getthreaddata(L));
    auto scriptedPaint = lua_newrive<ScriptedPaint>(L, context->factory);
    setPropertiesFromDefinitionTable(L, scriptedPaint, 1);
    return 1;
}

static int paint_newindex(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }

    auto scriptedPaint = lua_torive<ScriptedPaint>(L, 1);
    paint_set_value(L, scriptedPaint, atom, 3);

    return 0;
}

void ScriptedPaint::pushStyle(lua_State* L)
{
    switch (m_style)
    {
        case RenderPaintStyle::fill:
            lua_pushstring(L, "fill");
            break;
        case RenderPaintStyle::stroke:
            lua_pushstring(L, "stroke");
            break;
        default:
            RIVE_UNREACHABLE();
    }
}

void ScriptedPaint::pushJoin(lua_State* L)
{
    switch (m_join)
    {
        case StrokeJoin::miter:
            lua_pushstring(L, "miter");
            break;
        case StrokeJoin::bevel:
            lua_pushstring(L, "bevel");
            break;
        case StrokeJoin::round:
            lua_pushstring(L, "round");
            break;
        default:
            RIVE_UNREACHABLE();
    }
}

void ScriptedPaint::pushCap(lua_State* L)
{
    switch (m_cap)
    {
        case StrokeCap::butt:
            lua_pushstring(L, "butt");
            break;
        case StrokeCap::square:
            lua_pushstring(L, "square");
            break;
        case StrokeCap::round:
            lua_pushstring(L, "round");
            break;
        default:
            RIVE_UNREACHABLE();
    }
}

void ScriptedPaint::pushThickness(lua_State* L)
{
    lua_pushnumber(L, m_thickness);
}

void ScriptedPaint::pushBlendMode(lua_State* L)
{
    switch (m_blendMode)
    {
        case BlendMode::srcOver:
            lua_pushstring(L, "srcOver");
            break;
        case BlendMode::screen:
            lua_pushstring(L, "screen");
            break;
        case BlendMode::overlay:
            lua_pushstring(L, "overlay");
            break;
        case BlendMode::darken:
            lua_pushstring(L, "darken");
            break;
        case BlendMode::lighten:
            lua_pushstring(L, "lighten");
            break;
        case BlendMode::colorDodge:
            lua_pushstring(L, "colorDodge");
            break;
        case BlendMode::colorBurn:
            lua_pushstring(L, "colorBurn");
            break;
        case BlendMode::hardLight:
            lua_pushstring(L, "hardLight");
            break;
        case BlendMode::softLight:
            lua_pushstring(L, "softLight");
            break;
        case BlendMode::difference:
            lua_pushstring(L, "difference");
            break;
        case BlendMode::exclusion:
            lua_pushstring(L, "exclusion");
            break;
        case BlendMode::multiply:
            lua_pushstring(L, "multiply");
            break;
        case BlendMode::hue:
            lua_pushstring(L, "hue");
            break;
        case BlendMode::saturation:
            lua_pushstring(L, "saturation");
            break;
        case BlendMode::color:
            lua_pushstring(L, "color");
            break;
        case BlendMode::luminosity:
            lua_pushstring(L, "luminosity");
            break;
        default:
            RIVE_UNREACHABLE();
    }
}

void ScriptedPaint::pushFeather(lua_State* L) { lua_pushnumber(L, m_feather); }
void ScriptedPaint::pushGradient(lua_State* L)
{
    if (m_gradient)
    {
        ScriptedGradient* gradient = lua_newrive<ScriptedGradient>(L);
        gradient->shader = m_gradient;
    }
    else
    {
        lua_pushnil(L);
    }
}
void ScriptedPaint::pushColor(lua_State* L) { lua_pushunsigned(L, m_color); }

static int paint_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }

    auto scriptedPaint = lua_torive<ScriptedPaint>(L, 1);
    switch (atom)
    {
        case (int)LuaAtoms::style:
            scriptedPaint->pushStyle(L);
            return 1;
        case (int)LuaAtoms::join:
            scriptedPaint->pushJoin(L);
            return 1;
        case (int)LuaAtoms::cap:
            scriptedPaint->pushCap(L);
            return 1;
        case (int)LuaAtoms::thickness:
            scriptedPaint->pushThickness(L);
            return 1;
        case (int)LuaAtoms::blendMode:
            scriptedPaint->pushBlendMode(L);
            return 1;
        case (int)LuaAtoms::feather:
            scriptedPaint->pushFeather(L);
            return 1;
        case (int)LuaAtoms::gradient:
            scriptedPaint->pushGradient(L);
            return 1;
        case (int)LuaAtoms::color:
            scriptedPaint->pushColor(L);
            return 1;
        default:
            return 0;
    }
}

static int paint_copy(lua_State* L)
{
    int argCount = lua_gettop(L);

    auto scriptedPaint = lua_torive<ScriptedPaint>(L, 1);

    ScriptingContext* context =
        static_cast<ScriptingContext*>(lua_getthreaddata(L));
    auto scriptedPaintCopy =
        lua_newrive<ScriptedPaint>(L, context->factory, *scriptedPaint);

    if (argCount == 2)
    {
        setPropertiesFromDefinitionTable(L, scriptedPaintCopy, 2);
    }
    return 1;
}

static int paint_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        switch (atom)
        {
            case (int)LuaAtoms::copy:
                return paint_copy(L);
        }
    }

    luaL_error(L,
               "%s is not a valid method of %s",
               str,
               ScriptedMat2D::luaName);
    return 0;
}

static const luaL_Reg paintStaticMethods[] = {
    {"new", paint_new},
    {"with", paint_with},
    {NULL, NULL},
};

int luaopen_rive_paint(lua_State* L)
{
    luaL_register(L, ScriptedPaint::luaName, paintStaticMethods);
    lua_register_rive<ScriptedPaint>(L);

    lua_pushcfunction(L, paint_index, nullptr);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, paint_newindex, nullptr);
    lua_setfield(L, -2, "__newindex");

    lua_pushcfunction(L, paint_namecall, nullptr);
    lua_setfield(L, -2, "__namecall");

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1); // pop the metatable
    return 1;
}

#endif

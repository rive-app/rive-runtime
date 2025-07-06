#ifdef WITH_RIVE_SCRIPTING
#include "rive/math/vec2d.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/factory.hpp"

#include <math.h>
#include <stdio.h>

using namespace rive;

RenderPath* ScriptedPath::renderPath(lua_State* L)
{
    if (m_isRenderPathDirty)
    {
        m_isRenderPathDirty = false;
        if (!m_renderPath)
        {
            ScriptingContext* context =
                static_cast<ScriptingContext*>(lua_getthreaddata(L));
            m_renderPath = context->factory->makeEmptyRenderPath();
            m_renderPath->fillRule(FillRule::clockwise);
        }
        else
        {
            m_renderPath->rewind();
        }
        m_renderPath->addRawPath(rawPath);
    }

    return m_renderPath.get();
}

static ScriptedPath* lua_pushpath(lua_State* L)
{
    return lua_newrive<ScriptedPath>(L);
}

static int path_construct(lua_State* L)
{
    lua_pushpath(L);
    return 1;
}

static int path_moveTo(lua_State* L)
{
    auto scriptedPath = lua_torive<ScriptedPath>(L, 1);
    auto vec = lua_checkvec2d(L, 2);
    scriptedPath->rawPath.move(*vec);
    return 0;
}

static int path_lineTo(lua_State* L)
{
    auto scriptedPath = lua_torive<ScriptedPath>(L, 1);
    auto vec = lua_checkvec2d(L, 2);
    scriptedPath->rawPath.line(*vec);
    return 0;
}

static int path_quadTo(lua_State* L)
{
    auto scriptedPath = lua_torive<ScriptedPath>(L, 1);
    auto p1 = lua_checkvec2d(L, 2);
    auto p2 = lua_checkvec2d(L, 3);
    scriptedPath->rawPath.quad(*p1, *p2);
    return 0;
}

static int path_cubicTo(lua_State* L)
{
    auto scriptedPath = lua_torive<ScriptedPath>(L, 1);
    auto p1 = lua_checkvec2d(L, 2);
    auto p2 = lua_checkvec2d(L, 3);
    auto p3 = lua_checkvec2d(L, 4);
    scriptedPath->rawPath.cubic(*p1, *p2, *p3);
    return 0;
}

static int path_close(lua_State* L)
{
    auto scriptedPath = lua_torive<ScriptedPath>(L, 1);
    scriptedPath->rawPath.close();
    return 0;
}

static int path_reset(lua_State* L)
{
    auto scriptedPath = lua_torive<ScriptedPath>(L, 1);
    scriptedPath->rawPath.reset();
    return 0;
}

static int path_add(lua_State* L)
{
    auto scriptedPath = lua_torive<ScriptedPath>(L, 1);
    auto scriptedPathToAdd = lua_torive<ScriptedPath>(L, 2);
    // TODO: Mat2D at 3
    scriptedPath->rawPath.addPath(scriptedPathToAdd->rawPath);
    scriptedPath->markDirty();
    return 0;
}

static int path_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        switch (atom)
        {
            case (int)LuaAtoms::moveTo:
                return path_moveTo(L);
            case (int)LuaAtoms::lineTo:
                return path_lineTo(L);
            case (int)LuaAtoms::quadTo:
                return path_quadTo(L);
            case (int)LuaAtoms::cubicTo:
                return path_cubicTo(L);
            case (int)LuaAtoms::close:
                return path_close(L);
            case (int)LuaAtoms::reset:
                return path_reset(L);
            case (int)LuaAtoms::add:
                return path_add(L);
        }
    }

    luaL_error(L, "%s is not a valid method of %s", str, ScriptedPath::luaName);
    return 0;
}

static const luaL_Reg pathlib[] = {
    {NULL, NULL},
};

static void create_path_metatable(lua_State* L)
{
    lua_register_rive<ScriptedPath>(L);

    lua_pushcfunction(L, path_namecall, nullptr);
    lua_setfield(L, -2, "__namecall");
    // Create metatable for the metatable (so we can call it).
    lua_createtable(L, 0, 1);
    lua_pushcfunction(L, path_construct, nullptr);
    lua_setfield(L, -2, "__call");
    // -3 as it's the library (Path) that we're setting this metatable on.
    lua_setmetatable(L, -3);

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1); // pop the metatable
}

int luaopen_rive_path(lua_State* L)
{
    luaL_register(L, ScriptedPath::luaName, pathlib);
    create_path_metatable(L);

    return 1;
}

#endif

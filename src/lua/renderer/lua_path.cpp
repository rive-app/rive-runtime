#ifdef WITH_RIVE_SCRIPTING
#include "rive/math/vec2d.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/math/contour_measure.hpp"
#include "rive/math/path_measure.hpp"
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
            m_renderPath = context->factory()->makeEmptyRenderPath();
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

static int path_new(lua_State* L)
{
    lua_pushpath(L);
    return 1;
}

static int path_moveTo(lua_State* L)
{
    auto scriptedPath = lua_torive<ScriptedPath>(L, 1);
    auto vec = lua_checkvec2d(L, 2);
    scriptedPath->rawPath.move(*vec);
    scriptedPath->markDirty();
    return 0;
}

static int path_lineTo(lua_State* L)
{
    auto scriptedPath = lua_torive<ScriptedPath>(L, 1);
    auto vec = lua_checkvec2d(L, 2);
    scriptedPath->rawPath.line(*vec);
    scriptedPath->markDirty();
    return 0;
}

static int path_quadTo(lua_State* L)
{
    auto scriptedPath = lua_torive<ScriptedPath>(L, 1);
    auto p1 = lua_checkvec2d(L, 2);
    auto p2 = lua_checkvec2d(L, 3);
    scriptedPath->rawPath.quad(*p1, *p2);
    scriptedPath->markDirty();
    return 0;
}

static int path_cubicTo(lua_State* L)
{
    auto scriptedPath = lua_torive<ScriptedPath>(L, 1);
    auto p1 = lua_checkvec2d(L, 2);
    auto p2 = lua_checkvec2d(L, 3);
    auto p3 = lua_checkvec2d(L, 4);
    scriptedPath->rawPath.cubic(*p1, *p2, *p3);
    scriptedPath->markDirty();
    return 0;
}

static int path_close(lua_State* L)
{
    auto scriptedPath = lua_torive<ScriptedPath>(L, 1);
    scriptedPath->rawPath.close();
    scriptedPath->markDirty();
    return 0;
}

static int path_reset(lua_State* L)
{
    auto scriptedPath = lua_torive<ScriptedPath>(L, 1);
    scriptedPath->rawPath.reset();
    scriptedPath->markDirty();
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

static int path_contours(lua_State* L)
{
    auto scriptedPath = lua_torive<ScriptedPath>(L, 1);
    // Use the copy constructor to ensure ContourMeasure outlives the path
    auto iter = make_rcp<RefCntContourMeasureIter>(scriptedPath->rawPath);
    auto firstContour = iter->get()->next();
    if (firstContour)
    {
        lua_newrive<ScriptedContourMeasure>(L, firstContour, iter);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static int path_measure(lua_State* L)
{
    auto scriptedPath = lua_torive<ScriptedPath>(L, 1);
    PathMeasure pathMeasure(&scriptedPath->rawPath);
    lua_newrive<ScriptedPathMeasure>(L, std::move(pathMeasure));
    return 1;
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
            case (int)LuaAtoms::contours:
                return path_contours(L);
            case (int)LuaAtoms::measure:
                return path_measure(L);
        }
    }

    luaL_error(L, "%s is not a valid method of %s", str, ScriptedPath::luaName);
    return 0;
}

// ContourMeasure methods
static int contour_measure_length(lua_State* L)
{
    auto scripted = lua_torive<ScriptedContourMeasure>(L, 1);
    lua_pushnumber(L, scripted->measure()->length());
    return 1;
}

static int contour_measure_isClosed(lua_State* L)
{
    auto scripted = lua_torive<ScriptedContourMeasure>(L, 1);
    lua_pushboolean(L, scripted->measure()->isClosed());
    return 1;
}

static int contour_measure_positionAndTangent(lua_State* L)
{
    auto scripted = lua_torive<ScriptedContourMeasure>(L, 1);
    float distance = (float)luaL_checknumber(L, 2);
    auto posTan = scripted->measure()->getPosTan(distance);
    lua_pushvec2d(L, posTan.pos);
    lua_pushvec2d(L, posTan.tan);
    return 2;
}

static int contour_measure_warp(lua_State* L)
{
    auto scripted = lua_torive<ScriptedContourMeasure>(L, 1);
    auto src = lua_checkvec2d(L, 2);
    Vec2D result = scripted->measure()->warp(*src);
    lua_pushvec2d(L, result);
    return 1;
}

static int contour_measure_extract(lua_State* L)
{
    auto scripted = lua_torive<ScriptedContourMeasure>(L, 1);
    float startDistance = (float)luaL_checknumber(L, 2);
    float endDistance = (float)luaL_checknumber(L, 3);
    auto destPath = lua_torive<ScriptedPath>(L, 4);
    bool startWithMove = lua_isboolean(L, 5) ? lua_toboolean(L, 5) : true;
    scripted->measure()->getSegment(startDistance,
                                    endDistance,
                                    &destPath->rawPath,
                                    startWithMove);
    destPath->markDirty();
    return 0;
}

static int contour_measure_next(lua_State* L)
{
    auto scripted = lua_torive<ScriptedContourMeasure>(L, 1);
    auto iter = scripted->iter();
    if (iter)
    {
        auto nextContour = iter->get()->next();
        if (nextContour)
        {
            // Create new ScriptedContourMeasure with the same rcp iter
            // The iter is already advanced, so we can reuse it
            lua_newrive<ScriptedContourMeasure>(L, nextContour, iter);
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

static int contour_measure_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }

    switch (atom)
    {
        case (int)LuaAtoms::length:
            return contour_measure_length(L);
        case (int)LuaAtoms::isClosed:
            return contour_measure_isClosed(L);
        case (int)LuaAtoms::next:
            return contour_measure_next(L);
        default:
            return 0;
    }
}

static int contour_measure_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        switch (atom)
        {
            case (int)LuaAtoms::positionAndTangent:
                return contour_measure_positionAndTangent(L);
            case (int)LuaAtoms::warp:
                return contour_measure_warp(L);
            case (int)LuaAtoms::extract:
                return contour_measure_extract(L);
        }
    }

    luaL_error(L,
               "%s is not a valid method of %s",
               str,
               ScriptedContourMeasure::luaName);
    return 0;
}

// PathMeasure methods
static int path_measure_length(lua_State* L)
{
    auto scripted = lua_torive<ScriptedPathMeasure>(L, 1);
    lua_pushnumber(L, scripted->measure()->length());
    return 1;
}

static int path_measure_isClosed(lua_State* L)
{
    auto scripted = lua_torive<ScriptedPathMeasure>(L, 1);
    lua_pushboolean(L, scripted->measure()->isClosed());
    return 1;
}

static int path_measure_positionAndTangent(lua_State* L)
{
    auto scripted = lua_torive<ScriptedPathMeasure>(L, 1);
    float distance = (float)luaL_checknumber(L, 2);
    auto posTanDist = scripted->measure()->atDistance(distance);
    lua_pushvec2d(L, posTanDist.pos);
    lua_pushvec2d(L, posTanDist.tan);
    return 2;
}

static int path_measure_warp(lua_State* L)
{
    auto scripted = lua_torive<ScriptedPathMeasure>(L, 1);
    auto src = lua_checkvec2d(L, 2);
    // Use atDistance to get position and tangent, then apply warp formula
    auto posTanDist = scripted->measure()->atDistance(src->x);
    Vec2D result = {
        posTanDist.pos.x - posTanDist.tan.y * src->y,
        posTanDist.pos.y + posTanDist.tan.x * src->y,
    };
    lua_pushvec2d(L, result);
    return 1;
}

static int path_measure_extract(lua_State* L)
{
    auto scripted = lua_torive<ScriptedPathMeasure>(L, 1);
    float startDistance = (float)luaL_checknumber(L, 2);
    float endDistance = (float)luaL_checknumber(L, 3);
    auto destPath = lua_torive<ScriptedPath>(L, 4);
    bool startWithMove = lua_isboolean(L, 5) ? lua_toboolean(L, 5) : true;
    scripted->measure()->getSegment(startDistance,
                                    endDistance,
                                    &destPath->rawPath,
                                    startWithMove);
    destPath->markDirty();
    return 0;
}

static int path_measure_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }

    switch (atom)
    {
        case (int)LuaAtoms::length:
            return path_measure_length(L);
        case (int)LuaAtoms::isClosed:
            return path_measure_isClosed(L);
        default:
            return 0;
    }
}

static int path_measure_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        switch (atom)
        {
            case (int)LuaAtoms::positionAndTangent:
                return path_measure_positionAndTangent(L);
            case (int)LuaAtoms::warp:
                return path_measure_warp(L);
            case (int)LuaAtoms::extract:
                return path_measure_extract(L);
        }
    }

    luaL_error(L,
               "%s is not a valid method of %s",
               str,
               ScriptedPathMeasure::luaName);
    return 0;
}

static const luaL_Reg pathStaticMethods[] = {
    {"new", path_new},
    {NULL, NULL},
};

int luaopen_rive_path(lua_State* L)
{
    luaL_register(L, ScriptedPath::luaName, pathStaticMethods);
    lua_register_rive<ScriptedPath>(L);

    lua_pushcfunction(L, path_namecall, nullptr);
    lua_setfield(L, -2, "__namecall");

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1); // pop the metatable

    // Register ContourMeasure
    lua_register_rive<ScriptedContourMeasure>(L);
    lua_pushcfunction(L, contour_measure_index, nullptr);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, contour_measure_namecall, nullptr);
    lua_setfield(L, -2, "__namecall");
    lua_setreadonly(L, -1, true);
    lua_pop(L, 1); // pop the metatable

    // Register PathMeasure
    lua_register_rive<ScriptedPathMeasure>(L);
    lua_pushcfunction(L, path_measure_index, nullptr);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, path_measure_namecall, nullptr);
    lua_setfield(L, -2, "__namecall");
    lua_setreadonly(L, -1, true);
    lua_pop(L, 1); // pop the metatable

    return 1;
}

#endif

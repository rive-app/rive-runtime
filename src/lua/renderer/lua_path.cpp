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

ScriptedPathData::ScriptedPathData(const RawPath* path)
{
    rawPath.addPath(*path);
}

int ScriptedPathData::totalCommands() { return (int)rawPath.verbs().size(); }

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
    auto scriptedPath = (ScriptedPathData*)lua_touserdata(L, 1);
    auto scriptedPathToAdd = (ScriptedPathData*)lua_touserdata(L, 2);
    const Mat2D* transform = nullptr;
    int nargs = lua_gettop(L);
    if (nargs == 3)
    {
        auto matrix = lua_torive<ScriptedMat2D>(L, 3);
        transform = &matrix->value;
    }
    scriptedPath->rawPath.addPath(scriptedPathToAdd->rawPath, transform);
    scriptedPath->markDirty();
    return 0;
}

static int path_command(lua_State* L)
{
    auto scriptedPath = (ScriptedPathData*)lua_touserdata(L, 1);
    auto rawPath = scriptedPath->rawPath;
    auto verbs = rawPath.verbs();
    auto points = rawPath.points();
    auto verbIndex = int(luaL_checknumber(L, 2));
    auto verbName = "none";
    std::vector<Vec2D> verbPoints;

    if (verbIndex >= 0 && verbIndex < (int)verbs.size())
    {
        auto verb = verbs[verbIndex];

        // Calculate the point index for this verb by summing point counts of
        // previous verbs
        int pointIndex = 0;
        for (int i = 0; i < verbIndex; i++)
        {
            pointIndex += path_verb_to_point_count(verbs[i]);
        }

        switch (verb)
        {
            case PathVerb::move:
                verbName = "moveTo";
                if (pointIndex < (int)points.size())
                {
                    verbPoints.push_back(points[pointIndex]);
                }
                break;
            case PathVerb::line:
                verbName = "lineTo";
                if (pointIndex < (int)points.size())
                {
                    verbPoints.push_back(points[pointIndex]);
                }
                break;
            case PathVerb::quad:
                verbName = "quadTo";
                if (pointIndex + 1 < (int)points.size())
                {
                    verbPoints.push_back(points[pointIndex]);
                    verbPoints.push_back(points[pointIndex + 1]);
                }
                break;
            case PathVerb::cubic:
                verbName = "cubicTo";
                if (pointIndex + 2 < (int)points.size())
                {
                    verbPoints.push_back(points[pointIndex]);
                    verbPoints.push_back(points[pointIndex + 1]);
                    verbPoints.push_back(points[pointIndex + 2]);
                }
                break;
            case PathVerb::close:
                verbName = "close";
                // close has no points
                break;
        }
    }
    lua_newrive<ScriptedPathCommand>(L, verbName, verbPoints);
    return 1;
}

static int path_contours(lua_State* L)
{
    auto scriptedPath = (ScriptedPathData*)lua_touserdata(L, 1);
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
    auto scriptedPath = (ScriptedPathData*)lua_touserdata(L, 1);
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

static int path_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);

    // If it's not a string/atom, treat it as a numeric index
    if (!key)
    {
        int index = luaL_checkinteger(L, 2);
        // Convert from 1-based Lua index to 0-based C++ index
        // Replace the index at position 2 with the 0-based version
        lua_pushinteger(L, index - 1); // Push 0-based index
        lua_replace(L, 2);             // Replace the value at position 2
        // Now stack has: 1=path, 2=0-based index, which is what path_command
        // expects
        return path_command(L);
    }

    // String indices are handled by __namecall for methods
    return 0;
}

static int path_length(lua_State* L)
{
    auto scriptedPath = (ScriptedPathData*)lua_touserdata(L, 1);

    auto totalCommands = scriptedPath->totalCommands();
    lua_pushnumber(L, totalCommands);
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

static int property_namecall_atom(lua_State* L,
                                  ScriptedPathCommand* pathCommand,
                                  uint8_t tag,
                                  int atom,
                                  bool& error)
{
    switch (atom)
    {
        case (int)LuaAtoms::type:
        {
            lua_pushstring(L, pathCommand->type().c_str());
            return 1;
        }
    }
    error = true;
    return 0;
}

static int pathCommand_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);

    // If it's not a string/atom, treat it as a numeric index
    if (!key)
    {
        int luaIndex = luaL_checkinteger(L, 2);
        int index = luaIndex - 1;

        auto pathCommand = (ScriptedPathCommand*)lua_touserdata(L, 1);
        auto points = pathCommand->points();
        auto size = (int)points.size();
        if (index >= 0 && index < size)
        {
            auto point = points[index];
            lua_pushvec2d(L, point);
            return 1;
        }
    }
    else
    {
        int atom;
        lua_tostringatom(L, 2, &atom);

        size_t namelen = 0;
        const char* name = luaL_checklstring(L, 2, &namelen);

        auto tag = lua_userdatatag(L, 1);
        auto pathCommand = (ScriptedPathCommand*)lua_touserdata(L, 1);

        bool error = false;
        int stackChange =
            property_namecall_atom(L, pathCommand, tag, atom, error);
        if (!error)
        {
            return stackChange;
        }

        luaL_error(L, "'%s' is not a valid index of PathCommand", name);
        return 0;
    }
    return 0;
}

static int pathCommand_length(lua_State* L)
{
    auto pathCommand = (ScriptedPathCommand*)lua_touserdata(L, 1);

    auto points = pathCommand->points();
    lua_pushnumber(L, (int)points.size());
    return 1;
}

static int pathCommand_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);

    luaL_error(L,
               "%s is not a valid method of %s",
               str,
               ScriptedPathCommand::luaName);
    return 0;
}

static const luaL_Reg pathStaticMethods[] = {
    {"new", path_new},
    {NULL, NULL},
};

int luaopen_rive_path(lua_State* L)
{
    {
        lua_register_rive<ScriptedPathData>(L);

        lua_pushcfunction(L, path_index, nullptr);
        lua_setfield(L, -2, "__index");

        lua_pushcfunction(L, path_length, nullptr);
        lua_setfield(L, -2, "__len");

        lua_pushcfunction(L, path_namecall, nullptr);
        lua_setfield(L, -2, "__namecall");

        lua_setreadonly(L, -1, true);
        lua_pop(L, 1); // pop the metatable
    }
    {
        luaL_register(L, ScriptedPath::luaName, pathStaticMethods);
        lua_register_rive<ScriptedPath>(L);

        lua_pushcfunction(L, path_index, nullptr);
        lua_setfield(L, -2, "__index");

        lua_pushcfunction(L, path_length, nullptr);
        lua_setfield(L, -2, "__len");

        lua_pushcfunction(L, path_namecall, nullptr);
        lua_setfield(L, -2, "__namecall");

        lua_setreadonly(L, -1, true);
        lua_pop(L, 1); // pop the metatable
    }
    {
        lua_register_rive<ScriptedPathCommand>(L);

        lua_pushcfunction(L, pathCommand_index, nullptr);
        lua_setfield(L, -2, "__index");

        lua_pushcfunction(L, pathCommand_length, nullptr);
        lua_setfield(L, -2, "__len");

        lua_pushcfunction(L, pathCommand_namecall, nullptr);
        lua_setfield(L, -2, "__namecall");

        lua_setreadonly(L, -1, true);
        lua_pop(L, 1); // pop the metatable
    }

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


#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#include "lualib.h"

using namespace rive;

static int vec2d_index(lua_State* L)
{
    const float* vec = luaL_checkvector(L, 1);

    size_t namelen = 0;
    const char* name = luaL_checklstring(L, 2, &namelen);

    if (namelen == 1)
    {
        switch (name[0])
        {
            case 'x':
            case '1':
                lua_pushnumber(L, vec[0]);
                return 1;
            case 'y':
            case '2':
                lua_pushnumber(L, vec[1]);
                return 1;
            default:
                break;
        }
    }

    luaL_error(L, "'%s' is not a valid index of Vec2D", name);
    return 0;
}

static int vec2d_length(lua_State* L)
{
    auto vec = lua_checkvec2d(L, 1);
    lua_pushnumber(L, vec->length());
    return 1;
}

static int vec2d_normalized(lua_State* L)
{
    auto vec = lua_checkvec2d(L, 1);
    lua_pushvec2d(L, vec->normalized());
    return 1;
}

static int vec2d_lengthSquared(lua_State* L)
{
    auto vec = lua_checkvec2d(L, 1);
    lua_pushnumber(L, vec->lengthSquared());
    return 1;
}

static int vec2d_distance(lua_State* L)
{
    auto lhs = lua_checkvec2d(L, 1);
    auto rhs = lua_checkvec2d(L, 2);
    lua_pushnumber(L, Vec2D::distance(*lhs, *rhs));
    return 1;
}

static int vec2d_distanceSquared(lua_State* L)
{
    auto lhs = lua_checkvec2d(L, 1);
    auto rhs = lua_checkvec2d(L, 2);
    lua_pushnumber(L, Vec2D::distanceSquared(*lhs, *rhs));
    return 1;
}

static int vec2d_dot(lua_State* L)
{
    auto lhs = lua_checkvec2d(L, 1);
    auto rhs = lua_checkvec2d(L, 2);

    lua_pushnumber(L, Vec2D::dot(*lhs, *rhs));
    return 1;
}

static int vec2d_lerp(lua_State* L)
{
    auto lhs = lua_checkvec2d(L, 1);
    auto rhs = lua_checkvec2d(L, 2);
    float factor = float(luaL_checknumber(L, 3));
    lua_pushvec2d(L, Vec2D::lerp(*lhs, *rhs, factor));
    return 1;
}

static int vec2d_xy(lua_State* L)
{
    float x = (float)lua_tonumber(L, 1);
    float y = (float)lua_tonumber(L, 2);

    lua_pushvector2(L, x, y);
    return 1;
}

static int vec2d_origin(lua_State* L)
{
    lua_pushvector2(L, 0.0f, 0.0f);
    return 1;
}

static int vec2d_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        switch (atom)
        {
            case (int)LuaAtoms::length:
                return vec2d_length(L);
            case (int)LuaAtoms::lengthSquared:
                return vec2d_lengthSquared(L);
            case (int)LuaAtoms::normalized:
                return vec2d_normalized(L);
            case (int)LuaAtoms::distance:
                return vec2d_distance(L);
            case (int)LuaAtoms::distanceSquared:
                return vec2d_distanceSquared(L);
            case (int)LuaAtoms::dot:
                return vec2d_dot(L);
            case (int)LuaAtoms::lerp:
                return vec2d_lerp(L);
        }
    }

    luaL_error(L, "%s is not a valid method of Vec2D", luaL_checkstring(L, 1));
    return 0;
}

static const luaL_Reg vec2dStaticMethods[] = {
    {"distance", vec2d_distance},
    {"distanceSquared", vec2d_distanceSquared},
    {"dot", vec2d_dot},
    {"lerp", vec2d_lerp},
    {"xy", vec2d_xy},
    {"origin", vec2d_origin},
    {nullptr, nullptr}};

int luaopen_rive_vec2d(lua_State* L)
{
    luaL_register(L, "Vec2D", vec2dStaticMethods);
    // create metatable for T
    lua_createtable(L, 0, 1);

    // Dupe the metatable on the stack
    lua_pushvalue(L, -1);
    // Add a vector
    lua_pushvector(L, 0.0f, 0.0f, 0.0f);
    // Move it before the metatable
    lua_insert(L, -2);
    // Set metatable on the vector (globally sets the metatable for all
    // vectors).
    lua_setmetatable(L, -2);
    // pop vector
    lua_pop(L, 1);

    lua_pushcfunction(L, vec2d_index, nullptr);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, vec2d_namecall, nullptr);
    lua_setfield(L, -2, "__namecall");

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1); // pop the metatable

    return 1;
}

#endif

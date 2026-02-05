
#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#include "lualib.h"

using namespace rive;

static int vector_index(lua_State* L)
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

    luaL_error(L, "'%s' is not a valid index of Vector", name);
    return 0;
}

static int vector_length(lua_State* L)
{
    auto vec = lua_checkvec2d(L, 1);
    lua_pushnumber(L, vec->length());
    return 1;
}

static int vector_normalized(lua_State* L)
{
    auto vec = lua_checkvec2d(L, 1);
    lua_pushvec2d(L, vec->normalized());
    return 1;
}

static int vector_lengthSquared(lua_State* L)
{
    auto vec = lua_checkvec2d(L, 1);
    lua_pushnumber(L, vec->lengthSquared());
    return 1;
}

static int vector_distance(lua_State* L)
{
    auto lhs = lua_checkvec2d(L, 1);
    auto rhs = lua_checkvec2d(L, 2);
    lua_pushnumber(L, Vec2D::distance(*lhs, *rhs));
    return 1;
}

static int vector_distanceSquared(lua_State* L)
{
    auto lhs = lua_checkvec2d(L, 1);
    auto rhs = lua_checkvec2d(L, 2);
    lua_pushnumber(L, Vec2D::distanceSquared(*lhs, *rhs));
    return 1;
}

static int vector_dot(lua_State* L)
{
    auto lhs = lua_checkvec2d(L, 1);
    auto rhs = lua_checkvec2d(L, 2);

    lua_pushnumber(L, Vec2D::dot(*lhs, *rhs));
    return 1;
}

static int vector_cross(lua_State* L)
{
    auto lhs = lua_checkvec2d(L, 1);
    auto rhs = lua_checkvec2d(L, 2);
    lua_pushnumber(L, Vec2D::cross(*lhs, *rhs));
    return 1;
}

static int vector_scaleAndAdd(lua_State* L)
{
    auto a = lua_checkvec2d(L, 1);
    auto b = lua_checkvec2d(L, 2);
    float scale = float(luaL_checknumber(L, 3));
    lua_pushvec2d(L, Vec2D::scaleAndAdd(*a, *b, scale));
    return 1;
}

static int vector_scaleAndSub(lua_State* L)
{
    auto a = lua_checkvec2d(L, 1);
    auto b = lua_checkvec2d(L, 2);
    float scale = float(luaL_checknumber(L, 3));
    lua_pushvec2d(L, *a - *b * scale);
    return 1;
}

static int vector_lerp(lua_State* L)
{
    auto lhs = lua_checkvec2d(L, 1);
    auto rhs = lua_checkvec2d(L, 2);
    float factor = float(luaL_checknumber(L, 3));
    lua_pushvec2d(L, Vec2D::lerp(*lhs, *rhs, factor));
    return 1;
}

static int vector_xy(lua_State* L)
{
    float x = (float)lua_tonumber(L, 1);
    float y = (float)lua_tonumber(L, 2);

    lua_pushvector2(L, x, y);
    return 1;
}

static int vector_origin(lua_State* L)
{
    lua_pushvector2(L, 0.0f, 0.0f);
    return 1;
}

static int vector_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        switch (atom)
        {
            case (int)LuaAtoms::length:
                return vector_length(L);
            case (int)LuaAtoms::lengthSquared:
                return vector_lengthSquared(L);
            case (int)LuaAtoms::normalized:
                return vector_normalized(L);
            case (int)LuaAtoms::distance:
                return vector_distance(L);
            case (int)LuaAtoms::distanceSquared:
                return vector_distanceSquared(L);
            case (int)LuaAtoms::dot:
                return vector_dot(L);
            case (int)LuaAtoms::lerp:
                return vector_lerp(L);
        }
    }

    luaL_error(L, "%s is not a valid method of Vector", luaL_checkstring(L, 1));
    return 0;
}

static const luaL_Reg vectorStaticMethods[] = {
    {"distance", vector_distance},
    {"distanceSquared", vector_distanceSquared},
    {"dot", vector_dot},
    {"cross", vector_cross},
    {"scaleAndAdd", vector_scaleAndAdd},
    {"scaleAndSub", vector_scaleAndSub},
    {"lerp", vector_lerp},
    {"xy", vector_xy},
    {"origin", vector_origin},
    {"length", vector_length},
    {"lengthSquared", vector_lengthSquared},
    {"normalized", vector_normalized},
    {nullptr, nullptr}};

int luaopen_rive_vector(lua_State* L)
{
    luaL_register(L, "Vector", vectorStaticMethods);
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

    lua_pushcfunction(L, vector_index, nullptr);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, vector_namecall, nullptr);
    lua_setfield(L, -2, "__namecall");

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1); // pop the metatable

    return 1;
}

#endif

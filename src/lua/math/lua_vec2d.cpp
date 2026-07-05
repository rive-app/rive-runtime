
#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#include "lualib.h"
#include <cmath>
#include <cstring>

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
            case 'z':
            case '3':
                // Luau's vector type stores 3 components; .z is reachable
                // intrinsically through the VM's named-axis fastpath, but
                // numeric `[3]` indexing routes through this metamethod.
                lua_pushnumber(L, vec[2]);
                return 1;
            default:
                break;
        }
    }

    luaL_error(L, "'%s' is not a valid index of Vector", name);
    return 0;
}

// All magnitude and interpolation ops read all 3 components. For genuine 2D
// vectors z is 0, so results are identical to the old 2D math.
static inline float dot3(const float* a, const float* b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static int vector_length(lua_State* L)
{
    const float* vec = luaL_checkvector(L, 1);
    lua_pushnumber(L, std::sqrt(dot3(vec, vec)));
    return 1;
}

static int vector_normalized(lua_State* L)
{
    const float* vec = luaL_checkvector(L, 1);
    float len2 = dot3(vec, vec);
    float scale = len2 > 0 ? 1.f / std::sqrt(len2) : 1.f;
    lua_pushvector(L, vec[0] * scale, vec[1] * scale, vec[2] * scale);
    return 1;
}

static int vector_lengthSquared(lua_State* L)
{
    const float* vec = luaL_checkvector(L, 1);
    lua_pushnumber(L, dot3(vec, vec));
    return 1;
}

static int vector_distanceSquared(lua_State* L)
{
    const float* lhs = luaL_checkvector(L, 1);
    const float* rhs = luaL_checkvector(L, 2);
    float d[3] = {rhs[0] - lhs[0], rhs[1] - lhs[1], rhs[2] - lhs[2]};
    lua_pushnumber(L, dot3(d, d));
    return 1;
}

static int vector_distance(lua_State* L)
{
    const float* lhs = luaL_checkvector(L, 1);
    const float* rhs = luaL_checkvector(L, 2);
    float d[3] = {rhs[0] - lhs[0], rhs[1] - lhs[1], rhs[2] - lhs[2]};
    lua_pushnumber(L, std::sqrt(dot3(d, d)));
    return 1;
}

static int vector_dot(lua_State* L)
{
    const float* lhs = luaL_checkvector(L, 1);
    const float* rhs = luaL_checkvector(L, 2);
    lua_pushnumber(L, dot3(lhs, rhs));
    return 1;
}

static int vector_cross(lua_State* L)
{
    auto lhs = lua_checkvec2d(L, 1);
    auto rhs = lua_checkvec2d(L, 2);
    lua_pushnumber(L, Vec2D::cross(*lhs, *rhs));
    return 1;
}

// 3D cross product. Distinct from `cross`, which returns the scalar 2D
// perp-dot.
static int vector_cross3(lua_State* L)
{
    const float* a = luaL_checkvector(L, 1);
    const float* b = luaL_checkvector(L, 2);
    lua_pushvector(L,
                   a[1] * b[2] - a[2] * b[1],
                   a[2] * b[0] - a[0] * b[2],
                   a[0] * b[1] - a[1] * b[0]);
    return 1;
}

static int vector_scaleAndAdd(lua_State* L)
{
    const float* a = luaL_checkvector(L, 1);
    const float* b = luaL_checkvector(L, 2);
    float scale = float(luaL_checknumber(L, 3));
    lua_pushvector(L,
                   a[0] + b[0] * scale,
                   a[1] + b[1] * scale,
                   a[2] + b[2] * scale);
    return 1;
}

static int vector_scaleAndSub(lua_State* L)
{
    const float* a = luaL_checkvector(L, 1);
    const float* b = luaL_checkvector(L, 2);
    float scale = float(luaL_checknumber(L, 3));
    lua_pushvector(L,
                   a[0] - b[0] * scale,
                   a[1] - b[1] * scale,
                   a[2] - b[2] * scale);
    return 1;
}

// Exact at t=1, mirroring the VM fastcall's luai_lerpf so both dispatch
// paths return bit-identical results.
static inline float lerpf(float a, float b, float t)
{
    return t == 1.f ? b : a + (b - a) * t;
}

static int vector_lerp(lua_State* L)
{
    const float* a = luaL_checkvector(L, 1);
    const float* b = luaL_checkvector(L, 2);
    float t = float(luaL_checknumber(L, 3));
    lua_pushvector(L,
                   lerpf(a[0], b[0], t),
                   lerpf(a[1], b[1], t),
                   lerpf(a[2], b[2], t));
    return 1;
}

// vec:writeToBuffer(buf, byteOffset) — 12 bytes (x, y, z as float32).
static int vector_writeToBuffer(lua_State* L)
{
    const float* vec = luaL_checkvector(L, 1);
    size_t bufLen = 0;
    void* buf = luaL_checkbuffer(L, 2, &bufLen);
    int off = int(luaL_checkinteger(L, 3));
    if (off < 0 || size_t(off) + 12 > bufLen)
    {
        luaL_error(L, "Vector:writeToBuffer offset out of range");
        return 0;
    }
    std::memcpy(static_cast<uint8_t*>(buf) + off, vec, 12);
    return 0;
}

// vec:writeVec4(buf, byteOffset, w) — 16 bytes (x, y, z, w as float32),
// matching a vec4 uniform-buffer slot.
static int vector_writeVec4(lua_State* L)
{
    const float* vec = luaL_checkvector(L, 1);
    size_t bufLen = 0;
    void* buf = luaL_checkbuffer(L, 2, &bufLen);
    int off = int(luaL_checkinteger(L, 3));
    float w = float(luaL_checknumber(L, 4));
    if (off < 0 || size_t(off) + 16 > bufLen)
    {
        luaL_error(L, "Vector:writeVec4 offset out of range");
        return 0;
    }
    uint8_t* dst = static_cast<uint8_t*>(buf) + off;
    std::memcpy(dst, vec, 12);
    std::memcpy(dst + 12, &w, 4);
    return 0;
}

static int vector_xy(lua_State* L)
{
    float x = (float)lua_tonumber(L, 1);
    float y = (float)lua_tonumber(L, 2);

    lua_pushvector2(L, x, y);
    return 1;
}

static int vector_xyz(lua_State* L)
{
    float x = (float)lua_tonumber(L, 1);
    float y = (float)lua_tonumber(L, 2);
    float z = (float)lua_tonumber(L, 3);

    lua_pushvector(L, x, y, z);
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
            case (int)LuaAtoms::writeToBuffer:
                return vector_writeToBuffer(L);
            case (int)LuaAtoms::writeVec4:
                return vector_writeVec4(L);
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
    {"cross3", vector_cross3},
    {"scaleAndAdd", vector_scaleAndAdd},
    {"scaleAndSub", vector_scaleAndSub},
    {"lerp", vector_lerp},
    {"xy", vector_xy},
    {"xyz", vector_xyz},
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

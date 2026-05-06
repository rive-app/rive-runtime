#ifdef WITH_RIVE_SCRIPTING
#include "rive/math/mat4.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include <cstdlib>
#include <cstring>

using namespace rive;

static ScriptedMat4* lua_pushmat4(lua_State* L, const Mat4& mat)
{
    return lua_newrive<ScriptedMat4>(L, mat);
}

static ScriptedMat4* lua_pushmat4(lua_State* L)
{
    return lua_newrive<ScriptedMat4>(L);
}

// Mat4.values(c0x, c0y, c0z, c0w, c1x, ..., c3w) — column-major.
static int mat4_values(lua_State* L)
{
    auto out = lua_pushmat4(L);
    float* m = out->value.values();
    for (int i = 0; i < 16; ++i)
    {
        m[i] = float(luaL_checknumber(L, 1 + i));
    }
    return 1;
}

static int mat4_identity(lua_State* L)
{
    lua_pushmat4(L, Mat4::identity());
    return 1;
}

static int mat4_fromTranslation(lua_State* L)
{
    float x = float(luaL_checknumber(L, 1));
    float y = float(luaL_checknumber(L, 2));
    float z = float(luaL_checknumber(L, 3));
    lua_pushmat4(L, Mat4::fromTranslation(x, y, z));
    return 1;
}

static int mat4_fromScale(lua_State* L)
{
    float sx = float(luaL_checknumber(L, 1));
    float sy = lua_isnumber(L, 2) ? float(luaL_checknumber(L, 2)) : sx;
    float sz = lua_isnumber(L, 3) ? float(luaL_checknumber(L, 3)) : sx;
    lua_pushmat4(L, Mat4::fromScale(sx, sy, sz));
    return 1;
}

static int mat4_fromRotationX(lua_State* L)
{
    lua_pushmat4(L, Mat4::fromRotationX(float(luaL_checknumber(L, 1))));
    return 1;
}

static int mat4_fromRotationY(lua_State* L)
{
    lua_pushmat4(L, Mat4::fromRotationY(float(luaL_checknumber(L, 1))));
    return 1;
}

static int mat4_fromRotationZ(lua_State* L)
{
    lua_pushmat4(L, Mat4::fromRotationZ(float(luaL_checknumber(L, 1))));
    return 1;
}

static int mat4_perspective(lua_State* L)
{
    float fov = float(luaL_checknumber(L, 1));
    float aspect = float(luaL_checknumber(L, 2));
    float n = float(luaL_checknumber(L, 3));
    float f = float(luaL_checknumber(L, 4));
    lua_pushmat4(L, Mat4::perspective(fov, aspect, n, f, /*zeroToOne=*/true));
    return 1;
}

// Reverse-Z infinite-far perspective. See Mat4::perspectiveReverseZ.
static int mat4_perspectiveReverseZ(lua_State* L)
{
    float fov = float(luaL_checknumber(L, 1));
    float aspect = float(luaL_checknumber(L, 2));
    float n = float(luaL_checknumber(L, 3));
    lua_pushmat4(L, Mat4::perspectiveReverseZ(fov, aspect, n));
    return 1;
}

// In-place: Mat4.multiply(out, a, b)  ->  out = a * b. Returns out.
// Avoids per-call userdata allocation in tight loops.
static int mat4_static_multiply(lua_State* L)
{
    auto out = lua_torive<ScriptedMat4>(L, 1);
    auto a = lua_torive<ScriptedMat4>(L, 2);
    auto b = lua_torive<ScriptedMat4>(L, 3);
    out->value = Mat4::multiply(a->value, b->value);
    lua_pushvalue(L, 1);
    return 1;
}

// In-place: Mat4.multiplyAffine(out, a, b)  ->  out = a * b, assuming both
// inputs are affine (bottom row [0,0,0,1]). Faster than `multiply` (skips
// the bottom-row work).
static int mat4_static_multiplyAffine(lua_State* L)
{
    auto out = lua_torive<ScriptedMat4>(L, 1);
    auto a = lua_torive<ScriptedMat4>(L, 2);
    auto b = lua_torive<ScriptedMat4>(L, 3);
    out->value = Mat4::multiplyAffine(a->value, b->value);
    lua_pushvalue(L, 1);
    return 1;
}

static int mat4_static_invert(lua_State* L)
{
    auto out = lua_torive<ScriptedMat4>(L, 1);
    auto in = lua_torive<ScriptedMat4>(L, 2);
    lua_pushboolean(L, in->value.invert(&out->value));
    return 1;
}

// In-place: Mat4.invertAffine(out, in) — closed-form affine inverse.
// Returns true if invertible. Caller must ensure the input is affine.
static int mat4_static_invertAffine(lua_State* L)
{
    auto out = lua_torive<ScriptedMat4>(L, 1);
    auto in = lua_torive<ScriptedMat4>(L, 2);
    lua_pushboolean(L, in->value.invertAffine(&out->value));
    return 1;
}

// Field index lookup. Supports m11..m44 (row,col 1-indexed) and 1..16
// (column-major linear index, 1-indexed).
static int mat4_index_field(lua_State* L,
                            ScriptedMat4* mat,
                            const char* name,
                            size_t namelen)
{
    if (namelen == 3 && name[0] == 'm')
    {
        int row = name[1] - '0';
        int col = name[2] - '0';
        if (row >= 1 && row <= 4 && col >= 1 && col <= 4)
        {
            // m[row][col] 1-indexed; column-major storage means
            // index = (col-1)*4 + (row-1).
            lua_pushnumber(L, mat->value[(col - 1) * 4 + (row - 1)]);
            return 1;
        }
    }
    if (namelen >= 1 && namelen <= 2)
    {
        char* end = nullptr;
        long n = std::strtol(name, &end, 10);
        if (end && *end == '\0' && n >= 1 && n <= 16)
        {
            lua_pushnumber(L, mat->value[n - 1]);
            return 1;
        }
    }
    return 0;
}

static int mat4_newindex_field(lua_State* L,
                               ScriptedMat4* mat,
                               const char* name,
                               size_t namelen,
                               float value)
{
    if (namelen == 3 && name[0] == 'm')
    {
        int row = name[1] - '0';
        int col = name[2] - '0';
        if (row >= 1 && row <= 4 && col >= 1 && col <= 4)
        {
            mat->value[(col - 1) * 4 + (row - 1)] = value;
            return 0;
        }
    }
    if (namelen >= 1 && namelen <= 2)
    {
        char* end = nullptr;
        long n = std::strtol(name, &end, 10);
        if (end && *end == '\0' && n >= 1 && n <= 16)
        {
            mat->value[n - 1] = value;
            return 0;
        }
    }
    return -1;
}

static int mat4_index(lua_State* L)
{
    auto mat = lua_torive<ScriptedMat4>(L, 1);
    size_t namelen = 0;
    const char* name = luaL_checklstring(L, 2, &namelen);
    if (mat4_index_field(L, mat, name, namelen) == 1)
        return 1;
    luaL_error(L,
               "'%s' is not a valid index of %s",
               name,
               ScriptedMat4::luaName);
    return 0;
}

static int mat4_newindex(lua_State* L)
{
    auto mat = lua_torive<ScriptedMat4>(L, 1);
    size_t namelen = 0;
    const char* name = luaL_checklstring(L, 2, &namelen);
    float value = float(luaL_checknumber(L, 3));
    if (mat4_newindex_field(L, mat, name, namelen, value) == 0)
        return 0;
    luaL_error(L,
               "'%s' is not a valid index of %s",
               name,
               ScriptedMat4::luaName);
    return 0;
}

static int mat4_mul(lua_State* L)
{
    auto a = lua_torive<ScriptedMat4>(L, 1);
    auto b = lua_torive<ScriptedMat4>(L, 2);
    lua_pushmat4(L, Mat4::multiply(a->value, b->value));
    return 1;
}

static int mat4_eq(lua_State* L)
{
    auto a = lua_torive<ScriptedMat4>(L, 1);
    auto b = lua_torive<ScriptedMat4>(L, 2);
    lua_pushboolean(L, a->value == b->value);
    return 1;
}

static int mat4_invert(lua_State* L)
{
    auto mat = lua_torive<ScriptedMat4>(L, 1);
    Mat4 result;
    if (mat->value.invert(&result))
    {
        lua_pushmat4(L, result);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static int mat4_invertAffine(lua_State* L)
{
    auto mat = lua_torive<ScriptedMat4>(L, 1);
    Mat4 result;
    if (mat->value.invertAffine(&result))
    {
        lua_pushmat4(L, result);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static int mat4_transpose(lua_State* L)
{
    auto mat = lua_torive<ScriptedMat4>(L, 1);
    lua_pushmat4(L, mat->value.transposed());
    return 1;
}

// mat:transformPoint(x, y, z)  ->  vector(x', y', z')  (w=1, perspective
// divide)
static int mat4_transformPoint(lua_State* L)
{
    auto mat = lua_torive<ScriptedMat4>(L, 1);
    float x = float(luaL_checknumber(L, 2));
    float y = float(luaL_checknumber(L, 3));
    float z = float(luaL_checknumber(L, 4));
    float out[4];
    mat->value.transformVec4(out, x, y, z, 1.f);
    if (out[3] != 0.f && out[3] != 1.f)
    {
        float inv = 1.f / out[3];
        lua_pushvector(L, out[0] * inv, out[1] * inv, out[2] * inv);
    }
    else
    {
        lua_pushvector(L, out[0], out[1], out[2]);
    }
    return 1;
}

// mat:transformVec4(x, y, z, w)  ->  x', y', z', w' (no perspective divide)
// Useful for clip-space transforms where the caller wants the homogeneous w
// preserved.
static int mat4_transformVec4(lua_State* L)
{
    auto mat = lua_torive<ScriptedMat4>(L, 1);
    float x = float(luaL_checknumber(L, 2));
    float y = float(luaL_checknumber(L, 3));
    float z = float(luaL_checknumber(L, 4));
    float w = float(luaL_checknumber(L, 5));
    float out[4];
    mat->value.transformVec4(out, x, y, z, w);
    lua_pushnumber(L, out[0]);
    lua_pushnumber(L, out[1]);
    lua_pushnumber(L, out[2]);
    lua_pushnumber(L, out[3]);
    return 4;
}

// mat:writeToBuffer(buf, byteOffset)  — direct 64-byte memcpy of the
// column-major matrix into a Luau buffer (uniform-buffer-friendly).
static int mat4_writeToBuffer(lua_State* L)
{
    auto mat = lua_torive<ScriptedMat4>(L, 1);
    size_t bufLen = 0;
    void* buf = luaL_checkbuffer(L, 2, &bufLen);
    int off = int(luaL_checkinteger(L, 3));
    if (off < 0 || size_t(off) + 64 > bufLen)
    {
        luaL_error(L, "Mat4:writeToBuffer offset out of range");
        return 0;
    }
    std::memcpy(static_cast<uint8_t*>(buf) + off, mat->value.values(), 64);
    return 0;
}

static int mat4_namecall(lua_State* L)
{
    int atom;
    if (lua_namecallatom(L, &atom))
    {
        switch (atom)
        {
            case (int)LuaAtoms::invert:
                return mat4_invert(L);
            case (int)LuaAtoms::invertAffine:
                return mat4_invertAffine(L);
            case (int)LuaAtoms::transpose:
                return mat4_transpose(L);
            case (int)LuaAtoms::transformPoint:
                return mat4_transformPoint(L);
            case (int)LuaAtoms::transformVec4:
                return mat4_transformVec4(L);
            case (int)LuaAtoms::writeToBuffer:
                return mat4_writeToBuffer(L);
        }
    }
    luaL_error(L,
               "%s is not a valid method of %s",
               luaL_checkstring(L, 1),
               ScriptedMat4::luaName);
    return 0;
}

static const luaL_Reg mat4StaticMethods[] = {
    {"identity", mat4_identity},
    {"values", mat4_values},
    {"fromTranslation", mat4_fromTranslation},
    {"fromScale", mat4_fromScale},
    {"fromRotationX", mat4_fromRotationX},
    {"fromRotationY", mat4_fromRotationY},
    {"fromRotationZ", mat4_fromRotationZ},
    {"perspective", mat4_perspective},
    {"perspectiveReverseZ", mat4_perspectiveReverseZ},
    {"multiply", mat4_static_multiply},
    {"multiplyAffine", mat4_static_multiplyAffine},
    {"invert", mat4_static_invert},
    {"invertAffine", mat4_static_invertAffine},
    {nullptr, nullptr}};

int luaopen_rive_mat4(lua_State* L)
{
    luaL_register(L, ScriptedMat4::luaName, mat4StaticMethods);
    lua_register_rive<ScriptedMat4>(L);

    lua_pushcfunction(L, mat4_index, nullptr);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, mat4_newindex, nullptr);
    lua_setfield(L, -2, "__newindex");

    lua_pushcfunction(L, mat4_mul, nullptr);
    lua_setfield(L, -2, "__mul");

    lua_pushcfunction(L, mat4_eq, nullptr);
    lua_setfield(L, -2, "__eq");

    lua_pushcfunction(L, mat4_namecall, nullptr);
    lua_setfield(L, -2, "__namecall");

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1); // pop metatable
    return 1;
}

#endif

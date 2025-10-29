#ifdef WITH_RIVE_SCRIPTING
#include "rive/math/mat2d.hpp"
#include "rive/lua/rive_lua_libs.hpp"

using namespace rive;

static ScriptedMat2D* lua_pushmat2d(lua_State* L,
                                    float x1,
                                    float y1,
                                    float x2,
                                    float y2,
                                    float tx,
                                    float ty)
{
    return lua_newrive<ScriptedMat2D>(L, x1, y1, x2, y2, tx, ty);
}

static ScriptedMat2D* lua_pushmat2d(lua_State* L, const Mat2D& mat)
{
    return lua_newrive<ScriptedMat2D>(L, mat);
}

static int mat2d_values(lua_State* L)
{
    float m[6] = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
    for (int i = 0; i < 6; i++)
    {
        m[i] = float(luaL_checknumber(L, 1 + i));
    }

    lua_pushmat2d(L, m[0], m[1], m[2], m[3], m[4], m[5]);
    return 1;
}

static int mat2d_identity(lua_State* L)
{
    lua_pushmat2d(L, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
    return 1;
}

static int mat2d_withTranslation(lua_State* L)
{
    auto vec = lua_tovec2d(L, 1);
    if (vec != nullptr)
    {
        lua_pushmat2d(L, Mat2D::fromTranslation(*vec));
        return 1;
    }

    float x = float(luaL_checknumber(L, 1));
    float y = float(luaL_checknumber(L, 2));

    lua_pushmat2d(L, Mat2D::fromTranslation(Vec2D(x, y)));

    return 1;
}

static int mat2d_withRotation(lua_State* L)
{
    float radians = float(luaL_checknumber(L, 1));

    lua_pushmat2d(L, Mat2D::fromRotation(radians));

    return 1;
}

static int mat2d_withScale(lua_State* L)
{
    auto scale = lua_tovec2d(L, 1);
    if (scale != nullptr)
    {
        lua_pushmat2d(L, Mat2D::fromScale(scale->x, scale->y));
        return 1;
    }
    float scaleX = float(luaL_checknumber(L, 1));
    float scaleY = lua_isnumber(L, 2) ? float(luaL_checknumber(L, 2)) : scaleX;

    lua_pushmat2d(L, Mat2D::fromScale(scaleX, scaleY));

    return 1;
}

static int mat2d_withScaleAndTranslation(lua_State* L)
{
    auto scale = lua_tovec2d(L, 1);
    if (scale != nullptr)
    {
        auto translation = lua_checkvec2d(L, 2);
        lua_pushmat2d(L,
                      Mat2D::fromScaleAndTranslation(scale->x,
                                                     scale->y,
                                                     translation->x,
                                                     translation->y));
        return 1;
    }
    float scaleX = float(luaL_checknumber(L, 1));
    float scaleY = float(luaL_checknumber(L, 2));
    float translationX = float(luaL_checknumber(L, 3));
    float translationY = float(luaL_checknumber(L, 4));

    lua_pushmat2d(L,
                  Mat2D::fromScaleAndTranslation(scaleX,
                                                 scaleY,
                                                 translationX,
                                                 translationY));

    return 1;
}

static int mat2d_newindex(lua_State* L)
{
    auto mat = lua_torive<ScriptedMat2D>(L, 1);

    size_t namelen = 0;
    const char* name = luaL_checklstring(L, 2, &namelen);
    switch (namelen)
    {
        case 2:
            switch (name[0])
            {
                case 'x':
                    switch (name[1])
                    {
                        case 'x':
                            mat->value.xx(luaL_checknumber(L, 3));
                            return 0;
                        case 'y':
                            mat->value.xy(luaL_checknumber(L, 3));
                            return 0;
                    }
                    break;
                case 'y':
                    switch (name[1])
                    {
                        case 'x':
                            mat->value.yx(luaL_checknumber(L, 3));
                            return 0;
                        case 'y':
                            mat->value.yy(luaL_checknumber(L, 3));
                            return 0;
                    }
                    break;
                case 't':
                    switch (name[1])
                    {
                        case 'x':
                            mat->value.tx(luaL_checknumber(L, 3));
                            return 0;
                        case 'y':
                            mat->value.ty(luaL_checknumber(L, 3));
                            return 0;
                    }
                    return 1;
                default:
                    break;
            }
            break;
        case 1:
            switch (name[0])
            {
                case '1':
                    mat->value.xx(luaL_checknumber(L, 3));
                    return 0;
                case '2':
                    mat->value.xy(luaL_checknumber(L, 3));
                    return 0;
                case '3':
                    mat->value.yx(luaL_checknumber(L, 3));
                    return 0;
                case '4':
                    mat->value.yy(luaL_checknumber(L, 3));
                    return 0;
                case '5':
                    mat->value.tx(luaL_checknumber(L, 3));
                    return 0;
                case '6':
                    mat->value.ty(luaL_checknumber(L, 3));
                    return 0;
                default:
                    break;
            }
            break;
    }

    luaL_error(L,
               "'%s' is not a valid index of %s",
               name,
               ScriptedMat2D::luaName);
    return 0;
}

static int mat2d_index(lua_State* L)
{
    auto mat = lua_torive<ScriptedMat2D>(L, 1);

    size_t namelen = 0;
    const char* name = luaL_checklstring(L, 2, &namelen);
    switch (namelen)
    {
        case 2:
            switch (name[0])
            {
                case 'x':
                    switch (name[1])
                    {
                        case 'x':
                            lua_pushnumber(L, mat->value.xx());
                            return 1;
                        case 'y':
                            lua_pushnumber(L, mat->value.xy());
                            return 1;
                    }
                    break;
                case 'y':
                    switch (name[1])
                    {
                        case 'x':
                            lua_pushnumber(L, mat->value.yx());
                            return 1;
                        case 'y':
                            lua_pushnumber(L, mat->value.yy());
                            return 1;
                    }
                    break;
                case 't':
                    switch (name[1])
                    {
                        case 'x':
                            lua_pushnumber(L, mat->value.tx());
                            return 1;
                        case 'y':
                            lua_pushnumber(L, mat->value.ty());
                            return 1;
                    }
                    return 1;
                default:
                    break;
            }
            break;
        case 1:
            switch (name[0])
            {
                case '1':
                    lua_pushnumber(L, mat->value.xx());
                    return 1;
                case '2':
                    lua_pushnumber(L, mat->value.xy());
                    return 1;
                case '3':
                    lua_pushnumber(L, mat->value.yx());
                    return 1;
                case '4':
                    lua_pushnumber(L, mat->value.yy());
                    return 1;
                case '5':
                    lua_pushnumber(L, mat->value.tx());
                    return 1;
                case '6':
                    lua_pushnumber(L, mat->value.ty());
                    return 1;
                default:
                    break;
            }
            break;
    }

    luaL_error(L,
               "'%s' is not a valid index of %s",
               name,
               ScriptedMat2D::luaName);
    return 0;
}

static int mat2d_mul(lua_State* L)
{
    auto mat2d = lua_torive<ScriptedMat2D>(L, 1);
    auto vec = lua_tovec2d(L, 2);
    if (vec != nullptr)
    {
        lua_pushvec2d(L, mat2d->value * (*vec));
        return 1;
    }
    auto rhs = lua_torive<ScriptedMat2D>(L, 2);

    lua_pushmat2d(L, mat2d->value * rhs->value);
    return 1;
}

static int mat2d_invert(lua_State* L)
{
    auto mat2d = lua_torive<ScriptedMat2D>(L, 1);
    Mat2D result;
    if (mat2d->value.invert(&result))
    {
        lua_pushmat2d(L, result);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static int mat2d_isIdentity(lua_State* L)
{
    auto mat2d = lua_torive<ScriptedMat2D>(L, 1);
    lua_pushboolean(L, mat2d->value == Mat2D());
    return 1;
}

static int mat2d_eq(lua_State* L)
{
    auto lhs = lua_torive<ScriptedMat2D>(L, 1);
    auto rhs = lua_torive<ScriptedMat2D>(L, 2);
    lua_pushboolean(L, lhs->value == rhs->value);
    return 1;
}

static int mat2d_namecall(lua_State* L)
{
    int atom;
    if (lua_namecallatom(L, &atom))
    {
        switch (atom)
        {
            case (int)LuaAtoms::invert:
                return mat2d_invert(L);
            case (int)LuaAtoms::isIdentity:
                return mat2d_isIdentity(L);
        }
    }

    luaL_error(L,
               "%s is not a valid method of %s",
               luaL_checkstring(L, 1),
               ScriptedMat2D::luaName);
    return 0;
}

static int mat2d_static_invert(lua_State* L)
{
    auto out = lua_torive<ScriptedMat2D>(L, 1);
    auto in = lua_torive<ScriptedMat2D>(L, 2);

    if (in->value.invert(&out->value))
    {
        lua_pushboolean(L, 1);
    }
    else
    {
        lua_pushboolean(L, 0);
    }

    return 1;
}

static const luaL_Reg mat2dStaticMethods[] = {
    {"withTranslation", mat2d_withTranslation},
    {"withRotation", mat2d_withRotation},
    {"withScale", mat2d_withScale},
    {"withScaleAndTranslation", mat2d_withScaleAndTranslation},
    {"identity", mat2d_identity},
    {"values", mat2d_values},
    {"invert", mat2d_static_invert},
    {nullptr, nullptr}};

int luaopen_rive_mat2d(lua_State* L)
{
    luaL_register(L, ScriptedMat2D::luaName, mat2dStaticMethods);
    lua_register_rive<ScriptedMat2D>(L);

    lua_pushcfunction(L, mat2d_index, nullptr);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, mat2d_newindex, nullptr);
    lua_setfield(L, -2, "__newindex");

    lua_pushcfunction(L, mat2d_mul, nullptr);
    lua_setfield(L, -2, "__mul");

    lua_pushcfunction(L, mat2d_eq, nullptr);
    lua_setfield(L, -2, "__eq");

    lua_pushcfunction(L, mat2d_namecall, nullptr);
    lua_setfield(L, -2, "__namecall");

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1); // pop the metatable
    return 1;
}

#endif

#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"

#include <math.h>
#include <stdio.h>

using namespace rive;

ScriptedDataValue::~ScriptedDataValue()
{
    if (m_dataValue)
    {
        delete m_dataValue;
    }
}

static int property_namecall_atom(lua_State* L,
                                  ScriptedDataValue* scriptedDataValue,
                                  uint8_t tag,
                                  int atom,
                                  bool& error)
{
    switch (atom)
    {
        case (int)LuaAtoms::value:
        {
            assert(scriptedDataValue->state() == L);
            switch (tag)
            {
                case ScriptedDataValueNumber::luaTag:
                {
                    auto dataValue =
                        scriptedDataValue->dataValue()->as<DataValueNumber>();
                    lua_pushnumber(L, dataValue->value());
                    return 1;
                }
                case ScriptedDataValueString::luaTag:
                {
                    auto dataValue =
                        scriptedDataValue->dataValue()->as<DataValueString>();
                    lua_pushstring(L, dataValue->value().c_str());
                    return 1;
                }
                case ScriptedDataValueBoolean::luaTag:
                {
                    auto dataValue =
                        scriptedDataValue->dataValue()->as<DataValueBoolean>();
                    lua_pushboolean(L, dataValue->value() ? 1 : 0);
                    return 1;
                }
                case ScriptedDataValueColor::luaTag:
                {
                    auto dataValue =
                        scriptedDataValue->dataValue()->as<DataValueColor>();
                    lua_pushinteger(L, dataValue->value());
                    return 1;
                }
            }
        }
        case (int)LuaAtoms::red:
        {
            assert(scriptedDataValue->state() == L);
            switch (tag)
            {
                case ScriptedDataValueColor::luaTag:
                {
                    auto dataValue =
                        scriptedDataValue->dataValue()->as<DataValueColor>();
                    lua_pushinteger(L, dataValue->red());
                    return 1;
                }
            }
        }
        case (int)LuaAtoms::green:
        {
            assert(scriptedDataValue->state() == L);
            switch (tag)
            {
                case ScriptedDataValueColor::luaTag:
                {
                    auto dataValue =
                        scriptedDataValue->dataValue()->as<DataValueColor>();
                    lua_pushinteger(L, dataValue->green());
                    return 1;
                }
            }
        }
        case (int)LuaAtoms::blue:
        {
            assert(scriptedDataValue->state() == L);
            switch (tag)
            {
                case ScriptedDataValueColor::luaTag:
                {
                    auto dataValue =
                        scriptedDataValue->dataValue()->as<DataValueColor>();
                    lua_pushinteger(L, dataValue->blue());
                    return 1;
                }
            }
        }
        case (int)LuaAtoms::alpha:
        {
            assert(scriptedDataValue->state() == L);
            switch (tag)
            {
                case ScriptedDataValueColor::luaTag:
                {
                    auto dataValue =
                        scriptedDataValue->dataValue()->as<DataValueColor>();
                    lua_pushinteger(L, dataValue->alpha());
                    return 1;
                }
            }
        }
    }
    error = true;
    return 0;
}

static int data_value_index(lua_State* L)
{
    int atom;
    lua_tostringatom(L, 2, &atom);

    size_t namelen = 0;
    const char* name = luaL_checklstring(L, 2, &namelen);

    auto tag = lua_userdatatag(L, 1);
    auto scriptedDataValue = (ScriptedDataValue*)lua_touserdata(L, 1);

    bool error = false;
    int stackChange =
        property_namecall_atom(L, scriptedDataValue, tag, atom, error);
    if (!error)
    {
        return stackChange;
    }

    luaL_error(L, "'%s' is not a valid index of DataValue", name);
    return 0;
}

static int data_value_newindex(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }

    auto tag = lua_userdatatag(L, 1);
    auto scriptedDataValue = (ScriptedDataValue*)lua_touserdata(L, 1);
    switch (atom)
    {
        case (int)LuaAtoms::value:
        {
            switch (tag)
            {
                case ScriptedDataValueNumber::luaTag:
                {
                    auto val = float(luaL_checknumber(L, 3));
                    scriptedDataValue->dataValue()
                        ->as<DataValueNumber>()
                        ->value(val);
                    return 1;
                }
                case ScriptedDataValueString::luaTag:
                {
                    auto val = luaL_checkstring(L, 3);
                    scriptedDataValue->dataValue()
                        ->as<DataValueString>()
                        ->value(val);
                    return 1;
                }
                case ScriptedDataValueBoolean::luaTag:
                {
                    auto val = luaL_checkboolean(L, 3);
                    scriptedDataValue->dataValue()
                        ->as<DataValueBoolean>()
                        ->value(val != 0);
                    return 1;
                }
                case ScriptedDataValueColor::luaTag:
                {
                    auto val = luaL_checkinteger(L, 3);
                    scriptedDataValue->dataValue()->as<DataValueColor>()->value(
                        val);
                    return 1;
                }
            }
        }
        case (int)LuaAtoms::red:
        {
            switch (tag)
            {
                case ScriptedDataValueColor::luaTag:
                {
                    auto val = luaL_checkinteger(L, 3);
                    scriptedDataValue->dataValue()->as<DataValueColor>()->red(
                        val);
                    return 1;
                }
                default:
                    return 1;
            }
        }
        case (int)LuaAtoms::green:
        {
            switch (tag)
            {
                case ScriptedDataValueColor::luaTag:
                {
                    auto val = luaL_checkinteger(L, 3);
                    scriptedDataValue->dataValue()->as<DataValueColor>()->green(
                        val);
                    return 1;
                }
                default:
                    return 1;
            }
        }
        case (int)LuaAtoms::blue:
        {
            switch (tag)
            {
                case ScriptedDataValueColor::luaTag:
                {
                    auto val = luaL_checkinteger(L, 3);
                    scriptedDataValue->dataValue()->as<DataValueColor>()->blue(
                        val);
                    return 1;
                }
                default:
                    return 1;
            }
        }
        case (int)LuaAtoms::alpha:
        {
            switch (tag)
            {
                case ScriptedDataValueColor::luaTag:
                {
                    auto val = luaL_checkinteger(L, 3);
                    scriptedDataValue->dataValue()->as<DataValueColor>()->alpha(
                        val);
                    return 1;
                }
                default:
                    return 1;
            }
        }
        default:
            return 0;
    }

    return 0;
}

static int dataValue_number(lua_State* L)
{
    lua_newrive<ScriptedDataValueNumber>(L, L, 0);

    return 1;
}

static int dataValue_string(lua_State* L)
{
    lua_newrive<ScriptedDataValueString>(L, L, "");

    return 1;
}

static int dataValue_boolean(lua_State* L)
{
    lua_newrive<ScriptedDataValueBoolean>(L, L, false);

    return 1;
}

static int dataValue_color(lua_State* L)
{
    lua_newrive<ScriptedDataValueColor>(L, L, 0);

    return 1;
}

static int data_value_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        auto dataValue = (ScriptedDataValue*)lua_touserdata(L, 1);
        switch (atom)
        {
            case (int)LuaAtoms::isNumber:
            {
                assert(dataValue->state() == L);
                lua_pushboolean(L, dataValue->isNumber() ? 1 : 0);
                return 1;
            }
            case (int)LuaAtoms::isString:
            {
                assert(dataValue->state() == L);
                lua_pushboolean(L, dataValue->isString() ? 1 : 0);
                return 1;
            }
            case (int)LuaAtoms::isBoolean:
            {
                assert(dataValue->state() == L);
                lua_pushboolean(L, dataValue->isBoolean() ? 1 : 0);
                return 1;
            }
            case (int)LuaAtoms::isColor:
            {
                assert(dataValue->state() == L);
                lua_pushboolean(L, dataValue->isColor() ? 1 : 0);
                return 1;
            }
            default:
                break;
        }
    }

    luaL_error(L,
               "%s is not a valid method of %s",
               str,
               ScriptedPropertyViewModel::luaName);
    return 0;
}

static const luaL_Reg dataValueStaticMethods[] = {
    {"number", dataValue_number},
    {"string", dataValue_string},
    {"boolean", dataValue_boolean},
    {"color", dataValue_color},
    {nullptr, nullptr}};

int luaopen_rive_data_values(lua_State* L)
{
    {

        luaL_register(L, ScriptedDataValue::luaName, dataValueStaticMethods);
    }
    {
        lua_register_rive<ScriptedDataValueNumber>(L);

        lua_pushcfunction(L, data_value_index, nullptr);
        lua_setfield(L, -2, "__index");

        lua_pushcfunction(L, data_value_newindex, nullptr);
        lua_setfield(L, -2, "__newindex");

        lua_pushcfunction(L, data_value_namecall, nullptr);
        lua_setfield(L, -2, "__namecall");

        lua_setreadonly(L, -1, true);
        lua_pop(L, 1); // pop the metatable
    }
    {
        lua_register_rive<ScriptedDataValueString>(L);

        lua_pushcfunction(L, data_value_index, nullptr);
        lua_setfield(L, -2, "__index");

        lua_pushcfunction(L, data_value_newindex, nullptr);
        lua_setfield(L, -2, "__newindex");

        lua_pushcfunction(L, data_value_namecall, nullptr);
        lua_setfield(L, -2, "__namecall");

        lua_setreadonly(L, -1, true);
        lua_pop(L, 1); // pop the metatable
    }
    {
        lua_register_rive<ScriptedDataValueBoolean>(L);

        lua_pushcfunction(L, data_value_index, nullptr);
        lua_setfield(L, -2, "__index");

        lua_pushcfunction(L, data_value_newindex, nullptr);
        lua_setfield(L, -2, "__newindex");

        lua_pushcfunction(L, data_value_namecall, nullptr);
        lua_setfield(L, -2, "__namecall");

        lua_setreadonly(L, -1, true);
        lua_pop(L, 1); // pop the metatable
    }
    {
        lua_register_rive<ScriptedDataValueColor>(L);

        lua_pushcfunction(L, data_value_index, nullptr);
        lua_setfield(L, -2, "__index");

        lua_pushcfunction(L, data_value_newindex, nullptr);
        lua_setfield(L, -2, "__newindex");

        lua_pushcfunction(L, data_value_namecall, nullptr);
        lua_setfield(L, -2, "__namecall");

        lua_setreadonly(L, -1, true);
        lua_pop(L, 1); // pop the metatable
    }

    return 5;
}
#endif

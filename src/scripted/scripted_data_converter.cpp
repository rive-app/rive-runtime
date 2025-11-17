#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#endif
#include "rive/assets/script_asset.hpp"
#include "rive/component_dirt.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_bind_context.hpp"
#include "rive/data_bind/data_values/data_type.hpp"
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/data_bind/data_values/data_value_boolean.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include "rive/scripted/scripted_data_converter.hpp"

using namespace rive;

#ifdef WITH_RIVE_SCRIPTING
bool ScriptedDataConverter::scriptInit(LuaState* state)
{
    ScriptedObject::scriptInit(state);
    addScriptedDirt(ComponentDirt::Bindings);
    return true;
}

DataValue* ScriptedDataConverter::convert(DataValue* value, DataBind* dataBind)
{
    if (m_state == nullptr)
    {
        return value;
    }
    DataValue* dataValueResult = new DataValue();
    auto state = m_state->state;
    rive_lua_pushRef(state, m_self);
    if (static_cast<lua_Type>(lua_getfield(state, -1, "convert")) !=
        LUA_TFUNCTION)
    {
        fprintf(stderr, "expected convert to be a function\n");
    }
    else
    {
        if (value->is<DataValueNumber>())
        {
            lua_pushvalue(state, -2);
            lua_newrive<ScriptedDataValueNumber>(
                state,
                state,
                value->as<DataValueNumber>()->value());
        }
        else if (value->is<DataValueString>())
        {
            lua_pushvalue(state, -2);
            lua_newrive<ScriptedDataValueString>(
                state,
                state,
                value->as<DataValueString>()->value());
        }
        else if (value->is<DataValueBoolean>())
        {
            lua_pushvalue(state, -2);
            lua_newrive<ScriptedDataValueBoolean>(
                state,
                state,
                value->as<DataValueBoolean>()->value());
        }
        if (static_cast<lua_Status>(rive_lua_pcall(state, 2, 1)) != LUA_OK)
        {
            rive_lua_pop(state, 1);
        }
        else
        {
            auto result = (ScriptedDataValue*)lua_touserdata(state, -1);
            if (result->isNumber())
            {
                dataValueResult = new DataValueNumber(
                    result->dataValue()->as<DataValueNumber>()->value());
            }
            else if (result->isString())
            {
                dataValueResult = new DataValueString(
                    result->dataValue()->as<DataValueString>()->value());
            }
            else if (result->isBoolean())
            {
                dataValueResult = new DataValueBoolean(
                    result->dataValue()->as<DataValueBoolean>()->value());
            }
            rive_lua_pop(state, 2);
        }
    }
    return dataValueResult;
}
#endif

void ScriptedDataConverter::bindFromContext(DataContext* dataContext,
                                            DataBind* dataBind)
{
    m_dataContext = dataContext;
    reinit();
    Super::bindFromContext(dataContext, dataBind);
    for (auto prop : m_customProperties)
    {
        auto input = ScriptInput::from(prop);
        if (input != nullptr)
        {
            if (input->dataBind() != nullptr)
            {
                input->dataBind()->as<DataBindContext>()->bindFromContext(
                    dataContext);
            }
        }
    }
}

bool ScriptedDataConverter::advanceComponent(float elapsedSeconds,
                                             AdvanceFlags flags)
{
    if (elapsedSeconds == 0)
    {
        return false;
    }
    if ((flags & AdvanceFlags::AdvanceNested) == 0)
    {
        elapsedSeconds = 0;
    }
    return scriptAdvance(elapsedSeconds);
}

StatusCode ScriptedDataConverter::import(ImportStack& importStack)
{
    auto result = registerReferencer(importStack);
    if (result != StatusCode::Ok)
    {
        return result;
    }
    return Super::import(importStack);
}

Core* ScriptedDataConverter::clone() const
{
    ScriptedDataConverter* twin =
        ScriptedDataConverterBase::clone()->as<ScriptedDataConverter>();
    if (m_fileAsset != nullptr)
    {
        twin->setAsset(m_fileAsset);
    }
    for (auto prop : m_customProperties)
    {
        auto clonedValue = prop->clone()->as<CustomProperty>();
        twin->addProperty(clonedValue);
        auto scriptInput = ScriptInput::from(clonedValue);
        if (scriptInput != nullptr)
        {
            scriptInput->scriptedObject(twin);
            if (scriptInput->dataBind() != nullptr)
            {
                auto clonedDataBind =
                    scriptInput->dataBind()->clone()->as<DataBind>();
                clonedDataBind->target(clonedValue);
                scriptInput->dataBind(clonedDataBind);
                twin->addDirtyDataBind(clonedDataBind);
            }
        }
    }
    return twin;
}
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
#include "rive/data_bind/data_values/data_value_color.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include "rive/scripted/scripted_data_converter.hpp"

using namespace rive;

ScriptedDataConverter::~ScriptedDataConverter()
{
    disposeScriptInputs();
    if (m_dataValue)
    {
        delete m_dataValue;
    }
}

void ScriptedDataConverter::disposeScriptInputs()
{
    auto props = m_customProperties;
    ScriptedObject::disposeScriptInputs();
    for (auto prop : props)
    {
        auto scriptInput = ScriptInput::from(prop);
        if (scriptInput != nullptr)
        {
            // ScriptedDataConverters need to delete their own inputs
            // because they are not components
            delete scriptInput;
        }
    }
}

#ifdef WITH_RIVE_SCRIPTING
bool ScriptedDataConverter::scriptInit(lua_State* state)
{
    ScriptedObject::scriptInit(state);
    addScriptedDirt(ComponentDirt::Bindings);
    return true;
}

bool ScriptedDataConverter::pushDataValue(DataValue* value)
{
    // Stack: [self, field, self]
    if (value->is<DataValueNumber>())
    {
        lua_newrive<ScriptedDataValueNumber>(
            m_state,
            m_state,
            value->as<DataValueNumber>()->value());
    }
    else if (value->is<DataValueString>())
    {
        lua_newrive<ScriptedDataValueString>(
            m_state,
            m_state,
            value->as<DataValueString>()->value());
    }
    else if (value->is<DataValueBoolean>())
    {
        lua_newrive<ScriptedDataValueBoolean>(
            m_state,
            m_state,
            value->as<DataValueBoolean>()->value());
    }
    else if (value->is<DataValueColor>())
    {
        lua_newrive<ScriptedDataValueColor>(
            m_state,
            m_state,
            value->as<DataValueColor>()->value());
    }
    else
    {
        return false;
    }
    return true;
}

DataValue* ScriptedDataConverter::applyConversion(DataValue* value,
                                                  const std::string& method)
{
    if (m_state == nullptr)
    {
        return value;
    }

#ifdef WITH_RIVE_TOOLS
    if (!hasValidVM())
    {
        return value;
    }
#endif
    // Stack: []
    rive_lua_pushRef(m_state, m_self);
    // Stack: [self]
    lua_getfield(m_state, -1, method.c_str());

    // Stack: [self, field]
    lua_pushvalue(m_state, -2);
    // Stack: [self, field, self]
    if (pushDataValue(value))
    {
        // Stack: [self, field, self, ScriptedData]
        if (static_cast<lua_Status>(rive_lua_pcall(m_state, 2, 1)) == LUA_OK)
        {
            auto result = (ScriptedDataValue*)lua_touserdata(m_state, -1);
            if (result->isNumber())
            {
                storeData<DataValueNumber>(result->dataValue());
            }
            else if (result->isString())
            {
                storeData<DataValueString>(result->dataValue());
            }
            else if (result->isBoolean())
            {
                storeData<DataValueBoolean>(result->dataValue());
            }
            else if (result->isColor())
            {
                storeData<DataValueColor>(result->dataValue());
            }
        }
        // Stack: [self, status] or [self, result]
        rive_lua_pop(m_state, 2);
    }
    else
    {
        // Stack: [self, field, self]
        rive_lua_pop(m_state, 3);
    }
    if (!m_dataValue)
    {
        m_dataValue = new DataValue();
    }
    return m_dataValue;
}

DataValue* ScriptedDataConverter::convert(DataValue* value, DataBind* dataBind)
{
    if (dataConverts())
    {
        return applyConversion(value, "convert");
    }
    return value;
}

DataValue* ScriptedDataConverter::reverseConvert(DataValue* value,
                                                 DataBind* dataBind)
{
    if (dataReverseConverts())
    {
        return applyConversion(value, "reverseConvert");
    }
    return value;
}
#endif

void ScriptedDataConverter::bindFromContext(DataContext* dataContext,
                                            DataBind* dataBind)
{
    m_dataContext = dataContext;
    Super::bindFromContext(dataContext, dataBind);
    reinit();
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

void ScriptedDataConverter::addProperty(CustomProperty* prop)
{
    auto scriptInput = ScriptInput::from(prop);
    if (scriptInput != nullptr)
    {
        scriptInput->scriptedObject(this);
    }
    CustomPropertyContainer::addProperty(prop);
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
    }
    return twin;
}
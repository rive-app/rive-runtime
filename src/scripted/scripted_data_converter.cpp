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

void ScriptedDataConverter::didHydrateScriptInputs()
{
    addScriptedDirt(ComponentDirt::Bindings);
}

DataValue* ScriptedDataConverter::applyConversion(DataValue* value,
                                                  const std::string& method)
{
    if (m_vm == nullptr || !m_vm->valid() || m_self == 0)
    {
        return value;
    }
    ScriptBackend::ScriptDataResult result;
    if (!m_vm->callDataConvert(this, m_self, method.c_str(), value, &result))
    {
        // Assumed for legacy files but not implemented; pass the value through
        // unchanged (same as !dataConverts()).
        return value;
    }
    switch (result.kind)
    {
        case ScriptBackend::ScriptDataResult::Kind::number:
            storeData<DataValueNumber>(result.number);
            break;
        case ScriptBackend::ScriptDataResult::Kind::string:
            storeData<DataValueString>(result.string);
            break;
        case ScriptBackend::ScriptDataResult::Kind::boolean:
            storeData<DataValueBoolean>(result.boolean);
            break;
        case ScriptBackend::ScriptDataResult::Kind::color:
            storeData<DataValueColor>(result.color);
            break;
        case ScriptBackend::ScriptDataResult::Kind::none:
            break;
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
    m_dataContext = rcp<DataContext>(safe_ref(dataContext));
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
    if (!enums::is_flag_set(flags, AdvanceFlags::AdvanceNested))
    {
        elapsedSeconds = 0;
    }
    return advance(elapsedSeconds);
}

bool ScriptedDataConverter::advance(float elapsedSeconds)
{

    if (elapsedSeconds == 0)
    {
        return false;
    }
    auto needsAdvance = scriptAdvance(elapsedSeconds);
    if (needsAdvance)
    {
        markConverterDirty();
    }
    return needsAdvance;
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
        auto scriptedInputClone = ScriptInput::from(clonedValue);
        auto scriptedInputSource = ScriptInput::from(prop);
        auto thisDataBinds = dataBinds();
        auto twinDataBinds = twin->dataBinds();
        if (scriptedInputClone && scriptedInputSource)
        {
            if (scriptedInputSource->dataBind())
            {
                int index = 0;
                // Data binds are cloned in the data converters, and assigned to
                // the right target here
                for (auto& dataBind : thisDataBinds)
                {
                    if (dataBind->target() == prop)
                    {
                        if (index < twinDataBinds.size())
                        {
                            twinDataBinds[index]->target(clonedValue);
                            scriptedInputClone->dataBind(twinDataBinds[index]);
                        }
                    }
                    index++;
                }
            }
        }
    }
    return twin;
}

bool ScriptedDataConverter::addDataBindFromScriptedObject(DataBind* dataBind)
{
    addDataBind(dataBind);
    return true;
}
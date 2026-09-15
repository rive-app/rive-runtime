#include "rive/data_bind/context/context_value_string.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/scripted/scripted_transition.hpp"

using namespace rive;

DataBindContextValueString::DataBindContextValueString(DataBind* dataBind) :
    DataBindContextValue(dataBind)
{}

void DataBindContextValueString::apply(Core* target,
                                       uint32_t propertyKey,
                                       bool isMainDirection,
                                       DataBind* dataBind)
{
    syncSourceValue(dataBind);
    auto value = calculateValue<DataValueString, std::string>(m_dataValue,
                                                              isMainDirection,
                                                              dataBind);
    switch (CoreRegistry::propertyFieldId(propertyKey))
    {
        case CoreUintType::id:
        {
            if (target && target->is<Solo>())
            {
                target->as<Solo>()->updateByName(value);
            }
            else if (target && target->is<ScriptedTransition>())
            {
                target->as<ScriptedTransition>()->updateByName(value);
            }
            break;
        }
        default:
            CoreRegistry::setString(target, propertyKey, value);
            break;
    }
}
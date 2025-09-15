#include "rive/data_bind/context/context_value_string.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

DataBindContextValueString::DataBindContextValueString(DataBind* dataBind) :
    DataBindContextValue(dataBind)
{}

void DataBindContextValueString::apply(Core* target,
                                       uint32_t propertyKey,
                                       bool isMainDirection)
{
    syncSourceValue();
    auto value = calculateValue<DataValueString, std::string>(m_dataValue,
                                                              isMainDirection,
                                                              m_dataBind);
    switch (CoreRegistry::propertyFieldId(propertyKey))
    {
        case CoreUintType::id:
        {
            if (target && target->is<Solo>())
            {
                target->as<Solo>()->updateByName(value);
            }
            break;
        }
        default:
            CoreRegistry::setString(target, propertyKey, value);
            break;
    }
}
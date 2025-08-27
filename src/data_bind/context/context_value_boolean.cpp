#include "rive/data_bind/context/context_value_boolean.hpp"
#include "rive/data_bind/data_values/data_value_boolean.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

DataBindContextValueBoolean::DataBindContextValueBoolean(DataBind* dataBind) :
    DataBindContextValue(dataBind)
{}

void DataBindContextValueBoolean::apply(Core* target,
                                        uint32_t propertyKey,
                                        bool isMainDirection)
{
    syncSourceValue();
    auto value = calculateValue<DataValueBoolean, bool>(m_dataValue,
                                                        isMainDirection,
                                                        m_dataBind);
    CoreRegistry::setBool(target, propertyKey, value);
}

bool DataBindContextValueBoolean::syncTargetValue(Core* target,
                                                  uint32_t propertyKey)
{
    auto value = CoreRegistry::getBool(target, propertyKey);

    if (m_previousValue != value)
    {
        m_previousValue = value;
        m_targetDataValue.value(value);
        return true;
    }
    return false;
}
#include "rive/data_bind/context/context_value_trigger.hpp"
#include "rive/data_bind/data_values/data_value_trigger.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

DataBindContextValueTrigger::DataBindContextValueTrigger(DataBind* dataBind) :
    DataBindContextValue(dataBind)
{}

void DataBindContextValueTrigger::apply(Core* target,
                                        uint32_t propertyKey,
                                        bool isMainDirection)
{
    syncSourceValue();
    auto value = calculateValue<DataValueTrigger, uint32_t>(m_dataValue,
                                                            isMainDirection,
                                                            m_dataBind);
    CoreRegistry::setUint(target, propertyKey, value);
}

bool DataBindContextValueTrigger::syncTargetValue(Core* target,
                                                  uint32_t propertyKey)
{
    auto value = CoreRegistry::getUint(target, propertyKey);

    if (m_previousValue != value)
    {
        m_previousValue = value;
        m_targetDataValue.value(value);
        return true;
    }
    return false;
}
#include "rive/data_bind/context/context_value_trigger.hpp"
#include "rive/data_bind/data_values/data_value_trigger.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

DataBindContextValueTrigger::DataBindContextValueTrigger(DataBind* dataBind) :
    DataBindContextValue(dataBind)
{}

void DataBindContextValueTrigger::apply(Core* target,
                                        uint32_t propertyKey,
                                        bool isMainDirection,
                                        DataBind* dataBind)
{
    syncSourceValue(dataBind);
    auto value = calculateValue<DataValueTrigger, uint32_t>(m_dataValue,
                                                            isMainDirection,
                                                            dataBind);
    CoreRegistry::setUint(target, propertyKey, value);
}
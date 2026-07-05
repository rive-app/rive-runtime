#include "rive/data_bind/context/context_value_boolean.hpp"
#include "rive/data_bind/data_values/data_value_boolean.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

DataBindContextValueBoolean::DataBindContextValueBoolean(DataBind* dataBind) :
    DataBindContextValue(dataBind)
{}

void DataBindContextValueBoolean::apply(Core* target,
                                        uint32_t propertyKey,
                                        bool isMainDirection,
                                        DataBind* dataBind)
{
    syncSourceValue(dataBind);
    auto value = calculateValue<DataValueBoolean, bool>(m_dataValue,
                                                        isMainDirection,
                                                        dataBind);
    CoreRegistry::setBool(target, propertyKey, value);
}
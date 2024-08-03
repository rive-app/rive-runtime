#include "rive/data_bind/context/context_value_boolean.hpp"
#include "rive/data_bind/data_values/data_value_boolean.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

DataBindContextValueBoolean::DataBindContextValueBoolean(ViewModelInstanceValue* source,
                                                         DataConverter* converter) :
    DataBindContextValue(source, converter)
{}

void DataBindContextValueBoolean::apply(Core* target, uint32_t propertyKey, bool isMainDirection)
{
    updateSourceValue();
    auto value = calculateValue<DataValueBoolean, bool>(m_dataValue, isMainDirection);
    CoreRegistry::setBool(target, propertyKey, value);
}

DataValue* DataBindContextValueBoolean::getTargetValue(Core* target, uint32_t propertyKey)
{
    auto value = CoreRegistry::getBool(target, propertyKey);
    return new DataValueBoolean(value);
}
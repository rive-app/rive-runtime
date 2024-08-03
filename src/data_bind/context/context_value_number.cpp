#include "rive/data_bind/context/context_value_number.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

DataBindContextValueNumber::DataBindContextValueNumber(ViewModelInstanceValue* source,
                                                       DataConverter* converter) :
    DataBindContextValue(source, converter)
{}

void DataBindContextValueNumber::apply(Core* target, uint32_t propertyKey, bool isMainDirection)
{
    updateSourceValue();
    auto value = calculateValue<DataValueNumber, float>(m_dataValue, isMainDirection);
    CoreRegistry::setDouble(target, propertyKey, value);
}

DataValue* DataBindContextValueNumber::getTargetValue(Core* target, uint32_t propertyKey)
{
    auto value = CoreRegistry::getDouble(target, propertyKey);
    return new DataValueNumber(value);
}
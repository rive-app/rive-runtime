#include "rive/data_bind/context/context_value_number.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/generated/core_registry.hpp"
#include <cmath>

using namespace rive;

DataBindContextValueNumber::DataBindContextValueNumber(DataBind* dataBind) :
    DataBindContextValue(dataBind)
{}

void DataBindContextValueNumber::apply(Core* target,
                                       uint32_t propertyKey,
                                       bool isMainDirection)
{
    updateSourceValue();
    auto value = calculateValue<DataValueNumber, float>(m_dataValue,
                                                        isMainDirection,
                                                        m_dataBind);
    switch (CoreRegistry::propertyFieldId(propertyKey))
    {
        case CoreDoubleType::id:
            CoreRegistry::setDouble(target, propertyKey, value);
            break;
        case CoreUintType::id:
            int rounded = value < 0 ? 0 : std::round(value);
            CoreRegistry::setUint(target, propertyKey, rounded);
            break;
    }
}

DataValue* DataBindContextValueNumber::getTargetValue(Core* target,
                                                      uint32_t propertyKey)
{
    switch (CoreRegistry::propertyFieldId(propertyKey))
    {
        case CoreDoubleType::id:
        {
            auto value = CoreRegistry::getDouble(target, propertyKey);
            return new DataValueNumber(value);
        }
        break;
        case CoreUintType::id:
        {
            auto value = (float)CoreRegistry::getUint(target, propertyKey);
            return new DataValueNumber(value);
            break;
        }
    }
    return new DataValueNumber(0);
}
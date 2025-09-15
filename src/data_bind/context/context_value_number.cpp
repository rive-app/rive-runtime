#include "rive/data_bind/context/context_value_number.hpp"
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
    syncSourceValue();
    auto value = calculateValue<DataValueNumber, float>(m_dataValue,
                                                        isMainDirection,
                                                        m_dataBind);
    switch (CoreRegistry::propertyFieldId(propertyKey))
    {
        case CoreDoubleType::id:
            CoreRegistry::setDouble(target, propertyKey, value);
            break;
        case CoreUintType::id:

            if (target && target->is<Solo>())
            {
                target->as<Solo>()->updateByIndex((size_t)std::round(value));
            }
            else
            {
                int rounded = value < 0 ? 0 : std::round(value);
                CoreRegistry::setUint(target, propertyKey, rounded);
            }
            break;
    }
}
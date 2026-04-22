#include "rive/data_bind/context/context_value_enum.hpp"
#include "rive/data_bind/data_values/data_value_enum.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

DataBindContextValueEnum::DataBindContextValueEnum(DataBind* dataBind) :
    DataBindContextValue(dataBind)
{}

void DataBindContextValueEnum::apply(Core* target,
                                     uint32_t propertyKey,
                                     bool isMainDirection)
{

    syncSourceValue();
    auto value = calculateValue<DataValueEnum, uint32_t>(m_dataValue,
                                                         isMainDirection,
                                                         m_dataBind);
    switch (CoreRegistry::propertyFieldId(propertyKey))
    {
        case CoreUintType::id:
        {
            if (target && target->is<Solo>())
            {
                if (m_dataValue->is<DataValueEnum>())
                {
                    auto dataValueEnum = m_dataValue->as<DataValueEnum>();
                    auto dataEnum = dataValueEnum->dataEnum();
                    if (dataEnum)
                    {
                        auto valueString = dataEnum->value(value);
                        target->as<Solo>()->updateByName(valueString);
                    }
                }
            }
            else
            {
                CoreRegistry::setUint(target, propertyKey, value);
            }
            break;
        }
        default:
            CoreRegistry::setUint(target, propertyKey, value);
            break;
    }
}
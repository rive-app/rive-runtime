#include "rive/data_bind/context/context_value_enum.hpp"
#include "rive/data_bind/data_values/data_value_enum.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

DataBindContextValueEnum::DataBindContextValueEnum(ViewModelInstanceValue* source,
                                                   DataConverter* converter) :
    DataBindContextValue(source, converter)
{}

void DataBindContextValueEnum::apply(Core* target, uint32_t propertyKey, bool isMainDirection)
{

    updateSourceValue();
    auto value = calculateValue<DataValueEnum, uint32_t>(m_dataValue, isMainDirection);
    CoreRegistry::setUint(target, propertyKey, value);
}

DataValue* DataBindContextValueEnum::getTargetValue(Core* target, uint32_t propertyKey)
{
    auto value = CoreRegistry::getUint(target, propertyKey);
    auto viewmodelInstanceEnum = m_source->as<ViewModelInstanceEnum>();
    auto viewModelPropertyEnum =
        viewmodelInstanceEnum->viewModelProperty()->as<ViewModelPropertyEnum>();
    return new DataValueEnum(value, viewModelPropertyEnum->dataEnum());
}
#include "rive/data_bind/context/context_value_enum.hpp"
#include "rive/data_bind/data_values/data_value_enum.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

DataBindContextValueEnum::DataBindContextValueEnum(DataBind* dataBind) :
    DataBindContextValue(dataBind)
{
    auto source = m_dataBind->source();
    auto viewmodelInstanceEnum = source->as<ViewModelInstanceEnum>();
    auto viewModelPropertyEnum =
        viewmodelInstanceEnum->viewModelProperty()->as<ViewModelPropertyEnum>();
    m_targetDataValue.dataEnum(viewModelPropertyEnum->dataEnum());
}

void DataBindContextValueEnum::apply(Core* target,
                                     uint32_t propertyKey,
                                     bool isMainDirection)
{

    syncSourceValue();
    auto value = calculateValue<DataValueEnum, uint32_t>(m_dataValue,
                                                         isMainDirection,
                                                         m_dataBind);
    CoreRegistry::setUint(target, propertyKey, value);
}

bool DataBindContextValueEnum::syncTargetValue(Core* target,
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
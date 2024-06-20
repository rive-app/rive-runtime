#include "rive/data_bind/context/context_value_enum.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

DataBindContextValueEnum::DataBindContextValueEnum(ViewModelInstanceValue* value)
{
    m_Source = value;
}

void DataBindContextValueEnum::apply(Component* target, uint32_t propertyKey)
{
    auto enumSource = m_Source->as<ViewModelInstanceEnum>();
    auto enumProperty = enumSource->viewModelProperty()->as<ViewModelPropertyEnum>();
    auto enumValue = enumProperty->value(m_Source->as<ViewModelInstanceEnum>()->propertyValue());
    CoreRegistry::setString(target, propertyKey, enumValue);
}

void DataBindContextValueEnum::applyToSource(Component* target, uint32_t propertyKey)
{
    auto value = CoreRegistry::getString(target, propertyKey);
    auto enumSource = m_Source->as<ViewModelInstanceEnum>();
    auto enumProperty = enumSource->viewModelProperty()->as<ViewModelPropertyEnum>();
    auto valueIndex = enumProperty->valueIndex(value);
    if (valueIndex != -1 && m_Value != valueIndex)
    {
        m_Value = valueIndex;
        m_Source->as<ViewModelInstanceEnum>()->value(static_cast<uint32_t>(valueIndex));
    }
}
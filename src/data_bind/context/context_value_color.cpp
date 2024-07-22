#include "rive/data_bind/context/context_value_color.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

DataBindContextValueColor::DataBindContextValueColor(ViewModelInstanceValue* value)
{
    m_Source = value;
    m_Value = m_Source->as<ViewModelInstanceColor>()->propertyValue();
}

void DataBindContextValueColor::apply(Core* target, uint32_t propertyKey)
{
    CoreRegistry::setColor(target,
                           propertyKey,
                           m_Source->as<ViewModelInstanceColor>()->propertyValue());
}

void DataBindContextValueColor::applyToSource(Core* target, uint32_t propertyKey)
{
    auto value = CoreRegistry::getColor(target, propertyKey);
    if (m_Value != value)
    {
        m_Value = value;
        m_Source->as<ViewModelInstanceColor>()->propertyValue(value);
    }
}
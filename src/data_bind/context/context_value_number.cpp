#include "rive/data_bind/context/context_value_number.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

DataBindContextValueNumber::DataBindContextValueNumber(ViewModelInstanceValue* value)
{
    m_Source = value;
}

void DataBindContextValueNumber::apply(Component* target, uint32_t propertyKey)
{
    CoreRegistry::setDouble(target,
                            propertyKey,
                            m_Source->as<ViewModelInstanceNumber>()->propertyValue());
}

void DataBindContextValueNumber::applyToSource(Component* target, uint32_t propertyKey)
{
    auto value = CoreRegistry::getDouble(target, propertyKey);
    if (m_Value != value)
    {
        m_Value = value;
        m_Source->as<ViewModelInstanceNumber>()->propertyValue(value);
    }
}
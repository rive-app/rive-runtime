#include "rive/data_bind/context/context_value_number.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

DataBindContextValueNumber::DataBindContextValueNumber(ViewModelInstanceValue* value)
{
    m_Source = value;
    m_Value = m_Source->as<ViewModelInstanceNumber>()->propertyValue();
}

void DataBindContextValueNumber::apply(Core* target, uint32_t propertyKey)
{
    CoreRegistry::setDouble(target,
                            propertyKey,
                            m_Source->as<ViewModelInstanceNumber>()->propertyValue());
}

void DataBindContextValueNumber::applyToSource(Core* target, uint32_t propertyKey)
{
    auto value = CoreRegistry::getDouble(target, propertyKey);
    if (m_Value != value)
    {
        m_Value = value;
        m_Source->as<ViewModelInstanceNumber>()->propertyValue(value);
    }
}
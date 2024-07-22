#include "rive/data_bind/context/context_value_boolean.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

DataBindContextValueBoolean::DataBindContextValueBoolean(ViewModelInstanceValue* value)
{
    m_Source = value;
    m_Value = m_Source->as<ViewModelInstanceBoolean>()->propertyValue();
}

void DataBindContextValueBoolean::apply(Core* target, uint32_t propertyKey)
{
    CoreRegistry::setBool(target,
                          propertyKey,
                          m_Source->as<ViewModelInstanceBoolean>()->propertyValue());
}

void DataBindContextValueBoolean::applyToSource(Core* target, uint32_t propertyKey)
{
    auto value = CoreRegistry::getBool(target, propertyKey);
    if (m_Value != value)
    {
        m_Value = value;
        m_Source->as<ViewModelInstanceBoolean>()->propertyValue(value);
    }
}
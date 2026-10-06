#include "rive/custom_property_trigger.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"

using namespace rive;

void CustomPropertyTrigger::propertyValueChanged()
{
    m_changeSequence = ViewModelInstanceValue::nextChangeSequence();
}

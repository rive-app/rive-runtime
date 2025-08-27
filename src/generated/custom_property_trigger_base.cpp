#include "rive/generated/custom_property_trigger_base.hpp"
#include "rive/custom_property_trigger.hpp"

using namespace rive;

Core* CustomPropertyTriggerBase::clone() const
{
    auto cloned = new CustomPropertyTrigger();
    cloned->copy(*this);
    return cloned;
}

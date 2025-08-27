#include "rive/generated/animation/nested_trigger_base.hpp"
#include "rive/animation/nested_trigger.hpp"

using namespace rive;

Core* NestedTriggerBase::clone() const
{
    auto cloned = new NestedTrigger();
    cloned->copy(*this);
    return cloned;
}

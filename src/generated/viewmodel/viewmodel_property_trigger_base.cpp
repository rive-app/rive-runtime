#include "rive/generated/viewmodel/viewmodel_property_trigger_base.hpp"
#include "rive/viewmodel/viewmodel_property_trigger.hpp"

using namespace rive;

Core* ViewModelPropertyTriggerBase::clone() const
{
    auto cloned = new ViewModelPropertyTrigger();
    cloned->copy(*this);
    return cloned;
}

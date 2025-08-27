#include "rive/generated/viewmodel/viewmodel_instance_trigger_base.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"

using namespace rive;

Core* ViewModelInstanceTriggerBase::clone() const
{
    auto cloned = new ViewModelInstanceTrigger();
    cloned->copy(*this);
    return cloned;
}

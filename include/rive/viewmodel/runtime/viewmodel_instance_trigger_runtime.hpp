#ifndef _RIVE_VIEW_MODEL_INSTANCE_TRIGGER_RUNTIME_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_TRIGGER_RUNTIME_HPP_

#include <string>
#include <stdint.h>
#include "rive/viewmodel/runtime/viewmodel_instance_value_runtime.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"

namespace rive
{

class ViewModelInstanceTriggerRuntime : public ViewModelInstanceValueRuntime
{

public:
    ViewModelInstanceTriggerRuntime(
        ViewModelInstanceTrigger* viewModelInstance) :
        ViewModelInstanceValueRuntime(viewModelInstance)
    {}
    void trigger();
};
} // namespace rive
#endif

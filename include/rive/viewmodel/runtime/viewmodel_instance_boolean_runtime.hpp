#ifndef _RIVE_VIEW_MODEL_INSTANCE_BOOLEAN_RUNTIME_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_BOOLEAN_RUNTIME_HPP_

#include <string>
#include <stdint.h>
#include "rive/viewmodel/runtime/viewmodel_instance_value_runtime.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"

namespace rive
{

class ViewModelInstanceBooleanRuntime : public ViewModelInstanceValueRuntime
{

public:
    ViewModelInstanceBooleanRuntime(
        ViewModelInstanceBoolean* viewModelInstance) :
        ViewModelInstanceValueRuntime(viewModelInstance)
    {}
    bool value() const;
    void value(bool);
};
} // namespace rive
#endif

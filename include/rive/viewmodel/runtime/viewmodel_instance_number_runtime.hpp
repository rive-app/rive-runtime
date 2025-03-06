#ifndef _RIVE_VIEW_MODEL_INSTANCE_NUMBER_RUNTIME_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_NUMBER_RUNTIME_HPP_

#include <string>
#include <stdint.h>
#include "rive/viewmodel/runtime/viewmodel_instance_value_runtime.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"

namespace rive
{

class ViewModelInstanceNumberRuntime : public ViewModelInstanceValueRuntime
{

public:
    ViewModelInstanceNumberRuntime(ViewModelInstanceNumber* viewModelInstance) :
        ViewModelInstanceValueRuntime(viewModelInstance)
    {}
    float value() const;
    void value(float);
};
} // namespace rive
#endif

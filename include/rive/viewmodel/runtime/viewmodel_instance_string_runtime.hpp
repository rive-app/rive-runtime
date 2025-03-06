#ifndef _RIVE_VIEW_MODEL_INSTANCE_STRING_RUNTIME_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_STRING_RUNTIME_HPP_

#include <string>
#include <stdint.h>
#include "rive/viewmodel/runtime/viewmodel_instance_value_runtime.hpp"
#include "rive/viewmodel/viewmodel_instance_string.hpp"

namespace rive
{

class ViewModelInstanceStringRuntime : public ViewModelInstanceValueRuntime
{

public:
    ViewModelInstanceStringRuntime(ViewModelInstanceString* viewModelInstance) :
        ViewModelInstanceValueRuntime(viewModelInstance)
    {}
    const std::string& value() const;
    void value(std::string);
};
} // namespace rive
#endif

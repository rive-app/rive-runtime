#ifndef _RIVE_VIEW_MODEL_INSTANCE_COLOR_RUNTIME_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_COLOR_RUNTIME_HPP_

#include <string>
#include <stdint.h>
#include "rive/viewmodel/runtime/viewmodel_instance_value_runtime.hpp"
#include "rive/viewmodel/viewmodel_instance_color.hpp"

namespace rive
{

class ViewModelInstanceColorRuntime : public ViewModelInstanceValueRuntime
{

public:
    ViewModelInstanceColorRuntime(ViewModelInstanceColor* viewModelInstance) :
        ViewModelInstanceValueRuntime(viewModelInstance)
    {}
    int value() const;
    void value(int);
    void rgb(int, int, int);
    void argb(int, int, int, int);
    void alpha(int);
};
} // namespace rive
#endif

#ifndef _RIVE_VIEW_MODEL_INSTANCE_ARTBOARD_RUNTIME_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_ARTBOARD_RUNTIME_HPP_

#include <string>
#include <stdint.h>
#include "rive/viewmodel/runtime/viewmodel_instance_value_runtime.hpp"
#include "rive/viewmodel/viewmodel_instance_artboard.hpp"
#include "rive/bindable_artboard.hpp"

namespace rive
{

class ViewModelInstanceArtboardRuntime : public ViewModelInstanceValueRuntime
{

public:
    ViewModelInstanceArtboardRuntime(
        ViewModelInstanceArtboard* viewModelInstance) :
        ViewModelInstanceValueRuntime(viewModelInstance)
    {}
    void value(rcp<BindableArtboard> bindableArtboard);
    const DataType dataType() override { return DataType::artboard; }

#ifdef TESTING
    rcp<BindableArtboard> testing_value();
#endif
};
} // namespace rive
#endif


#include "rive/viewmodel/runtime/viewmodel_instance_artboard_runtime.hpp"

// Default namespace for Rive Cpp code
using namespace rive;

void ViewModelInstanceArtboardRuntime::value(
    rcp<BindableArtboard> bindableArtboard)
{
    auto* artboardValue =
        m_viewModelInstanceValue->as<ViewModelInstanceArtboard>();
    // Runtime rebinding should clear any previously bound instance so switching
    // to a new artboard without a VMI will not accidentally keep using stale
    // data.
    artboardValue->viewModelInstance(nullptr);
    artboardValue->asset(bindableArtboard);
}

void ViewModelInstanceArtboardRuntime::viewModelInstance(
    rcp<ViewModelInstance> viewModelInstance)
{
    m_viewModelInstanceValue->as<ViewModelInstanceArtboard>()
        ->viewModelInstance(viewModelInstance);
}

#ifdef TESTING
rcp<BindableArtboard> ViewModelInstanceArtboardRuntime::testing_value()
{
    return m_viewModelInstanceValue->as<ViewModelInstanceArtboard>()->asset();
}
#endif

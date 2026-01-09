
#include "rive/viewmodel/runtime/viewmodel_instance_artboard_runtime.hpp"

// Default namespace for Rive Cpp code
using namespace rive;

void ViewModelInstanceArtboardRuntime::value(
    rcp<BindableArtboard> bindableArtboard)
{
    m_viewModelInstanceValue->as<ViewModelInstanceArtboard>()->asset(
        bindableArtboard);
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

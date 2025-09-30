
#include "rive/viewmodel/runtime/viewmodel_instance_artboard_runtime.hpp"

// Default namespace for Rive Cpp code
using namespace rive;

void ViewModelInstanceArtboardRuntime::value(
    rcp<BindableArtboard> bindableArtboard)
{
    m_viewModelInstanceValue->as<ViewModelInstanceArtboard>()->asset(
        bindableArtboard);
}

#ifdef TESTING
rcp<BindableArtboard> ViewModelInstanceArtboardRuntime::testing_value()
{
    return m_viewModelInstanceValue->as<ViewModelInstanceArtboard>()->asset();
}
#endif

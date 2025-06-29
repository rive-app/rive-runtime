
#include "rive/viewmodel/runtime/viewmodel_instance_artboard_runtime.hpp"

// Default namespace for Rive Cpp code
using namespace rive;

void ViewModelInstanceArtboardRuntime::value(Artboard* artboard)
{
    m_viewModelInstanceValue->as<ViewModelInstanceArtboard>()->asset(artboard);
}

#ifdef TESTING
Artboard* ViewModelInstanceArtboardRuntime::testing_value()
{
    return m_viewModelInstanceValue->as<ViewModelInstanceArtboard>()->asset();
}
#endif

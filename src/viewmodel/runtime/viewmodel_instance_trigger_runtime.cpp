
#include "rive/viewmodel/runtime/viewmodel_instance_trigger_runtime.hpp"

// Default namespace for Rive Cpp code
using namespace rive;

void ViewModelInstanceTriggerRuntime::trigger()
{
    return m_viewModelInstanceValue->as<ViewModelInstanceTrigger>()->trigger();
}
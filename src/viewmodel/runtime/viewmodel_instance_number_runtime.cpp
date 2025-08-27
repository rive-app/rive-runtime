
#include "rive/viewmodel/runtime/viewmodel_instance_number_runtime.hpp"

// Default namespace for Rive Cpp code
using namespace rive;

float ViewModelInstanceNumberRuntime::value() const
{
    return m_viewModelInstanceValue->as<ViewModelInstanceNumber>()
        ->propertyValue();
}

void ViewModelInstanceNumberRuntime::value(float val)
{
    m_viewModelInstanceValue->as<ViewModelInstanceNumber>()->propertyValue(val);
}

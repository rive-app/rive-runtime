
#include "rive/viewmodel/runtime/viewmodel_instance_boolean_runtime.hpp"

// Default namespace for Rive Cpp code
using namespace rive;

bool ViewModelInstanceBooleanRuntime::value() const
{
    return m_viewModelInstanceValue->as<ViewModelInstanceBoolean>()
        ->propertyValue();
}

void ViewModelInstanceBooleanRuntime::value(bool val)
{
    m_viewModelInstanceValue->as<ViewModelInstanceBoolean>()->propertyValue(
        val);
}

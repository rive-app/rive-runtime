
#include "rive/viewmodel/runtime/viewmodel_instance_string_runtime.hpp"

// Default namespace for Rive Cpp code
using namespace rive;

const std::string& ViewModelInstanceStringRuntime::value() const
{
    return m_viewModelInstanceValue->as<ViewModelInstanceString>()
        ->propertyValue();
}

void ViewModelInstanceStringRuntime::value(std::string val)
{
    m_viewModelInstanceValue->as<ViewModelInstanceString>()->propertyValue(val);
}

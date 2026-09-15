
#include "rive/viewmodel/runtime/viewmodel_instance_list_index_runtime.hpp"

// Default namespace for Rive Cpp code
using namespace rive;

uint32_t ViewModelInstanceListIndexRuntime::value() const
{
    return m_viewModelInstanceValue->as<ViewModelInstanceSymbolListIndex>()
        ->propertyValue();
}

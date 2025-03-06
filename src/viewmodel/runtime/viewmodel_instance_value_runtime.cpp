
#include "rive/viewmodel/runtime/viewmodel_instance_value_runtime.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"

// Default namespace for Rive Cpp code
using namespace rive;

ViewModelInstanceValueRuntime::~ViewModelInstanceValueRuntime()
{
    if (m_viewModelInstanceValue != nullptr)
    {
        m_viewModelInstanceValue->removeDependent(this);
    }
}

ViewModelInstanceValueRuntime::ViewModelInstanceValueRuntime(
    ViewModelInstanceValue* instanceValue) :
    m_viewModelInstanceValue(instanceValue)
{
    instanceValue->addDependent(this);
}

void ViewModelInstanceValueRuntime::addDirt(ComponentDirt dirt, bool recurse)
{
    m_hasChanged = true;
}

void ViewModelInstanceValueRuntime::clearChanges() { m_hasChanged = false; }

bool ViewModelInstanceValueRuntime::flushChanges()
{
    if (m_hasChanged)
    {
        m_hasChanged = false;
        return true;
    }
    return false;
}

const std::string& ViewModelInstanceValueRuntime::name() const
{
    return m_viewModelInstanceValue->name();
}

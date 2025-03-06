#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/importers/viewmodel_instance_importer.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/refcnt.hpp"

using namespace rive;

StatusCode ViewModelInstanceValue::import(ImportStack& importStack)
{
    auto viewModelInstanceImporter =
        importStack.latest<ViewModelInstanceImporter>(
            ViewModelInstance::typeKey);
    if (viewModelInstanceImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    viewModelInstanceImporter->addValue(this);

    return Super::import(importStack);
}

void ViewModelInstanceValue::viewModelProperty(ViewModelProperty* value)
{
    m_ViewModelProperty = value;
}
ViewModelProperty* ViewModelInstanceValue::viewModelProperty()
{
    return m_ViewModelProperty;
}

void ViewModelInstanceValue::addDependent(Dirtyable* value)
{
    m_DependencyHelper.addDependent(value);
}

void ViewModelInstanceValue::removeDependent(Dirtyable* value)
{
    m_DependencyHelper.removeDependent(value);
}

void ViewModelInstanceValue::addDirt(ComponentDirt value)
{
    m_DependencyHelper.addDirt(value);
}

void ViewModelInstanceValue::setRoot(rcp<ViewModelInstance> viewModelInstance)
{
    m_DependencyHelper.dependecyRoot(&viewModelInstance);
}

std::string ViewModelInstanceValue::defaultName = "";

const std::string& ViewModelInstanceValue::name() const
{
    if (m_ViewModelProperty != nullptr)
    {
        return m_ViewModelProperty->constName();
    };
    return defaultName;
}
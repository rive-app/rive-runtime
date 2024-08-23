#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/importers/viewmodel_instance_importer.hpp"
#include "rive/data_bind/data_bind.hpp"

using namespace rive;

StatusCode ViewModelInstanceValue::import(ImportStack& importStack)
{
    auto viewModelInstanceImporter =
        importStack.latest<ViewModelInstanceImporter>(ViewModelInstance::typeKey);
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
ViewModelProperty* ViewModelInstanceValue::viewModelProperty() { return m_ViewModelProperty; }

void ViewModelInstanceValue::addDependent(DataBind* value)
{
    m_DependencyHelper.addDependent(value);
}

void ViewModelInstanceValue::addDirt(ComponentDirt value) { m_DependencyHelper.addDirt(value); }

void ViewModelInstanceValue::setRoot(ViewModelInstance* viewModelInstance)
{
    m_DependencyHelper.dependecyRoot(viewModelInstance);
}
#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"
#include "rive/importers/viewmodel_importer.hpp"
#include "rive/core_context.hpp"

using namespace rive;

void ViewModelInstance::addValue(ViewModelInstanceValue* value)
{
    m_PropertyValues.push_back(value);
}

ViewModelInstanceValue* ViewModelInstance::propertyValue(const uint32_t id)
{
    for (auto value : m_PropertyValues)
    {
        if (value->viewModelPropertyId() == id)
        {
            return value;
        }
    }
    return nullptr;
}

ViewModelInstanceValue* ViewModelInstance::propertyValue(const std::string& name)
{
    auto viewModelProperty = viewModel()->property(name);
    if (viewModelProperty != nullptr)
    {
        for (auto value : m_PropertyValues)
        {
            if (value->viewModelProperty() == viewModelProperty)
            {
                return value;
            }
        }
    }
    return nullptr;
}

void ViewModelInstance::viewModel(ViewModel* value) { m_ViewModel = value; }

ViewModel* ViewModelInstance::viewModel() const { return m_ViewModel; }

void ViewModelInstance::onComponentDirty(Component* component) {}

void ViewModelInstance::setAsRoot() { setRoot(this); }

void ViewModelInstance::setRoot(ViewModelInstance* value)
{
    for (auto propertyValue : m_PropertyValues)
    {
        propertyValue->setRoot(value);
    }
}

std::vector<ViewModelInstanceValue*> ViewModelInstance::propertyValues()
{
    return m_PropertyValues;
}

Core* ViewModelInstance::clone() const
{
    auto cloned = new ViewModelInstance();
    cloned->copy(*this);
    for (auto propertyValue : m_PropertyValues)
    {
        auto clonedValue = propertyValue->clone()->as<ViewModelInstanceValue>();
        cloned->addValue(clonedValue);
    }
    cloned->viewModel(viewModel());
    return cloned;
}

StatusCode ViewModelInstance::import(ImportStack& importStack)
{
    auto viewModelImporter = importStack.latest<ViewModelImporter>(ViewModel::typeKey);
    if (viewModelImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }

    viewModelImporter->addInstance(this);
    return StatusCode::Ok;
}

ViewModelInstanceValue* ViewModelInstance::propertyFromPath(std::vector<uint32_t>* path,
                                                            size_t index)
{
    if (index < path->size())
    {
        auto propertyId = (*path)[index];
        auto property = propertyValue(propertyId);
        if (property != nullptr)
        {
            if (index == path->size() - 1)
            {
                return property;
            }
            if (property->is<ViewModelInstanceViewModel>())
            {
                auto propertyViewModel = property->as<ViewModelInstanceViewModel>();
                auto viewModelInstance = propertyViewModel->referenceViewModelInstance();
                return viewModelInstance->propertyFromPath(path, index + 1);
            }
        }
    }
    return nullptr;
}
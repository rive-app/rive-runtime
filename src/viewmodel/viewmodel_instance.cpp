#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"
#include "rive/importers/viewmodel_importer.hpp"
#include "rive/viewmodel/viewmodel_property_viewmodel.hpp"
#include "rive/core_context.hpp"
#include "rive/refcnt.hpp"

using namespace rive;

ViewModelInstance::~ViewModelInstance()
{
    for (auto& value : m_PropertyValues)
    {
        if (value->is<ViewModelInstanceViewModel>())
        {
            auto vmInstanceViewModel = value->as<ViewModelInstanceViewModel>();
            if (vmInstanceViewModel->referenceViewModelInstance())
            {
                vmInstanceViewModel->referenceViewModelInstance()->removeParent(
                    this);
            }
        }
    }
    m_PropertyValues.clear();
    if (m_ViewModel != nullptr)
    {
        m_ViewModel->unref();
    }
}

void ViewModelInstance::addValue(ViewModelInstanceValue* value)
{
    m_PropertyValues.push_back(rcp<ViewModelInstanceValue>(value));
}

ViewModelInstanceValue* ViewModelInstance::propertyValue(const uint32_t id)
{
    for (auto value : m_PropertyValues)
    {
        if (value->viewModelPropertyId() == id)
        {
            return value.get();
        }
    }
    return nullptr;
}

bool ViewModelInstance::replaceViewModelByName(const std::string& name,
                                               rcp<ViewModelInstance> value)
{
    auto viewModelProperty = viewModel()->property(name);
    if (viewModelProperty != nullptr)
    {
        for (auto propertyValue : m_PropertyValues)
        {
            if (propertyValue->viewModelProperty() == viewModelProperty)
            {
                if (value->viewModelId() ==
                    viewModelProperty->as<ViewModelPropertyViewModel>()
                        ->viewModelReferenceId())
                {
                    propertyValue->as<ViewModelInstanceViewModel>()
                        ->referenceViewModelInstance(value);
                    rebindDependents();
                    return true;
                }
                break;
            }
        }
    }
    return false;
}

ViewModelInstanceValue* ViewModelInstance::propertyValue(
    const SymbolType symbolType)
{
    auto viewModelProperty = viewModel()->property(symbolType);
    if (viewModelProperty != nullptr)
    {
        for (auto value : m_PropertyValues)
        {
            if (value->viewModelProperty() == viewModelProperty)
            {
                return value.get();
            }
        }
    }
    return nullptr;
}

ViewModelInstanceValue* ViewModelInstance::propertyValue(
    const std::string& name)
{
    auto viewModelProperty = viewModel()->property(name);
    if (viewModelProperty != nullptr)
    {
        for (auto value : m_PropertyValues)
        {
            if (value->viewModelProperty() == viewModelProperty)
            {
                return value.get();
            }
        }
    }
    return nullptr;
}

void ViewModelInstance::viewModel(ViewModel* value)
{
    if (m_ViewModel != nullptr)
    {
        m_ViewModel->unref();
    }
    value->ref();
    m_ViewModel = value;
}

ViewModel* ViewModelInstance::viewModel() const { return m_ViewModel; }

void ViewModelInstance::onComponentDirty(Component* component) {}

void ViewModelInstance::setAsRoot(rcp<ViewModelInstance> instance)
{
    setRoot(instance);
}

void ViewModelInstance::setRoot(rcp<ViewModelInstance> value)
{
    for (auto propertyValue : m_PropertyValues)
    {
        propertyValue->setRoot(value);
    }
}

std::vector<rcp<ViewModelInstanceValue>> ViewModelInstance::propertyValues()
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
    auto viewModelImporter =
        importStack.latest<ViewModelImporter>(ViewModel::typeKey);
    if (viewModelImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }

    viewModelImporter->addInstance(this);
    return StatusCode::Ok;
}

ViewModelInstanceValue* ViewModelInstance::propertyFromPath(
    std::vector<uint32_t>* path,
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
                auto propertyViewModel =
                    property->as<ViewModelInstanceViewModel>();
                auto viewModelInstance =
                    propertyViewModel->referenceViewModelInstance();
                return viewModelInstance->propertyFromPath(path, index + 1);
            }
        }
    }
    return nullptr;
}

void ViewModelInstance::advanced()
{
    for (auto value : m_PropertyValues)
    {
        value->advanced();
    }
}

ViewModelInstanceValue* ViewModelInstance::symbol(int coreType)
{
    for (auto& value : m_PropertyValues)
    {
        if (value->coreType() == coreType)
        {
            return value.get();
        }
    }
    return nullptr;
}

void ViewModelInstance::addParent(ViewModelInstance* parent)
{
    if (!parent)
    {
        return;
    }
    auto p = std::find(m_parents.begin(), m_parents.end(), parent);
    if (p == m_parents.end())
    {
        m_parents.push_back(parent);
    }
}

void ViewModelInstance::removeParent(ViewModelInstance* parent)
{
    m_parents.erase(std::remove(m_parents.begin(), m_parents.end(), parent),
                    m_parents.end());
}

void ViewModelInstance::addDependent(DataBindContainer* dependent)
{
    if (!dependent)
    {
        return;
    }
    auto p = std::find(m_dependents.begin(), m_dependents.end(), dependent);
    if (p == m_dependents.end())
    {
        m_dependents.push_back(dependent);
    }
}

void ViewModelInstance::removeDependent(DataBindContainer* dependent)
{
    m_dependents.erase(
        std::remove(m_dependents.begin(), m_dependents.end(), dependent),
        m_dependents.end());
}

void ViewModelInstance::rebindDependents()
{
    for (auto& dependent : m_dependents)
    {
        dependent->rebind();
    }
    for (auto& parent : m_parents)
    {
        parent->rebindDependents();
    }
}

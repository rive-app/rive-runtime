#include <sstream>
#include <iomanip>
#include <array>
#include <algorithm>

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

bool ViewModelInstanceValue::hasChanged()
{
    return m_changeFlags & ValueFlags::valueChanged;
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

void ViewModelInstanceValue::advanced()
{
    m_usedLayers.clear();
    m_changeFlags &= ~ValueFlags::valueChanged;
}

void ViewModelInstanceValue::addDelegate(
    ViewModelInstanceValueDelegate* delegate)
{
    m_delegates.push_back(delegate);
    m_changeFlags |= ValueFlags::delegatesChanged;
}

void ViewModelInstanceValue::removeDelegate(
    ViewModelInstanceValueDelegate* delegate)
{
    auto itr = std::find(m_delegates.begin(), m_delegates.end(), delegate);
    if (itr == m_delegates.end())
    {
        return;
    }
    m_delegates.erase(itr);
    m_changeFlags |= ValueFlags::delegatesChanged;
}

bool ViewModelInstanceValue::suppressDelegation()
{
    if (m_changeFlags & ValueFlags::delegating)
    {
        return false;
    }
    m_changeFlags |= ValueFlags::delegating;
    return true;
}

void ViewModelInstanceValue::restoreDelegation()
{
    m_changeFlags &= ~ValueFlags::delegating;
}

void ViewModelInstanceValue::onValueChanged()
{
    m_changeFlags |= ValueFlags::valueChanged;

    if (m_changeFlags & ValueFlags::delegatesChanged)
    {
        m_delegatesCopy.clear();
        std::copy(m_delegates.begin(),
                  m_delegates.end(),
                  std::back_inserter(m_delegatesCopy));
        m_changeFlags &= ~ValueFlags::delegatesChanged;
    }

    if (m_changeFlags & ValueFlags::delegating)
    {
        // We're already calling delegates for this value change, changing the
        // value in a delegate will not report the change.
        return;
    }

    // Flag to guard against calling this recursively.
    m_changeFlags |= ValueFlags::delegating;

    // Iterate over a copy of the delegates in case delegates get changed in
    // valueChanged callbacks.
    for (auto delegate : m_delegatesCopy)
    {
        delegate->valueChanged();
    }

    // Clear guard flag.
    m_changeFlags &= ~ValueFlags::delegating;
}
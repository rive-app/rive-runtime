#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance_artboard.hpp"
#include "rive/component_dirt.hpp"
#include "rive/refcnt.hpp"

using namespace rive;

void ViewModelInstanceArtboard::propertyValueChanged()
{
    m_bindableArtboard = nullptr;
    addDirt(ComponentDirt::Bindings);
#ifdef WITH_RIVE_TOOLS
    if (m_changedCallback != nullptr)
    {
        m_changedCallback(this, propertyValue());
    }
#endif
    onValueChanged();
}

void ViewModelInstanceArtboard::asset(rcp<BindableArtboard> value)
{
    // Scripts may reassign the same value every frame, and each notification
    // rebuilds whatever mounts it.
    if (propertyValue() == -1 && value == m_bindableArtboard &&
        !m_boundViewModelInstanceChanged)
    {
        return;
    }
    m_boundViewModelInstanceChanged = false;
    {
        // Moving to the sentinel drops the stored bindable and notifies
        // before the new one is stored, so delegates wait for the single
        // notification below. A fresh property, or one already holding a
        // bindable, is at the sentinel already and relies on that
        // notification alone.
        SuppressDelegation suppress(this);
        propertyValue(-1);
    }
    m_bindableArtboard = value;
    addDirt(ComponentDirt::Bindings);
    onValueChanged();
}

void ViewModelInstanceArtboard::applyValue(DataValueInteger* dataValue)
{
    propertyValue(dataValue->value());
}

void ViewModelInstanceArtboard::boundViewModelInstance(
    rcp<ViewModelInstance> value)
{
    if (value != m_boundViewModelInstance)
    {
        m_boundViewModelInstance = value;
        m_boundViewModelInstanceChanged = true;
    }
}

void ViewModelInstanceArtboard::advanced()
{
    if (m_boundViewModelInstance != nullptr)
    {
        m_boundViewModelInstance->advanced();
    }

    ViewModelInstanceValue::advanced();
}
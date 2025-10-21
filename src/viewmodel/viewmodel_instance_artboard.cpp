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
    propertyValue(-1);
    m_bindableArtboard = value;
    addDirt(ComponentDirt::Bindings);
}

void ViewModelInstanceArtboard::applyValue(DataValueInteger* dataValue)
{
    propertyValue(dataValue->value());
}
#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance_trigger.hpp"
#include "rive/component_dirt.hpp"

using namespace rive;

void ViewModelInstanceTrigger::propertyValueChanged()
{
    addDirt(ComponentDirt::Bindings);
#ifdef WITH_RIVE_TOOLS
    if (m_changedCallback != nullptr)
    {
        m_changedCallback(this, propertyValue());
    }
#endif
    onValueChanged();
}

void ViewModelInstanceTrigger::advanced()
{
    SuppressDelegation suppress(this);
    propertyValue(0);

    ViewModelInstanceValue::advanced();
}

void ViewModelInstanceTrigger::applyValue(DataValueInteger* dataValue)
{
    propertyValue(dataValue->value());
}
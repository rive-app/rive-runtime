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
}

void ViewModelInstanceTrigger::advanced()
{
    propertyValue(0);
    m_usedLayers.clear();
}
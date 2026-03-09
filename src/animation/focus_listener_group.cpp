#include "rive/animation/focus_listener_group.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_listener.hpp"
#include "rive/focus_data.hpp"

using namespace rive;

FocusListenerGroup::FocusListenerGroup(
    FocusData* focusData,
    const StateMachineListener* listener,
    StateMachineInstance* stateMachineInstance) :
    m_focusData(focusData),
    m_listener(listener),
    m_stateMachineInstance(stateMachineInstance),
    m_isFocusListener(listener->listenerType() == ListenerType::focus)
{
    // Register ourselves as a listener on the FocusData
    m_focusData->addFocusListener(this);
}

FocusListenerGroup::~FocusListenerGroup()
{
    m_focusData->removeFocusListener(this);
}

void FocusListenerGroup::onFocused()
{
    // Only queue if this is a focus listener
    if (m_isFocusListener)
    {
        m_stateMachineInstance->queueFocusEvent(this, true);
    }
}

void FocusListenerGroup::onBlurred()
{
    // Only queue if this is a blur listener
    if (!m_isFocusListener)
    {
        m_stateMachineInstance->queueFocusEvent(this, false);
    }
}

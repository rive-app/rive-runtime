#include "rive/animation/keyboard_listener_group.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_listener.hpp"
#include "rive/focus_data.hpp"

using namespace rive;

KeyboardListenerGroup::KeyboardListenerGroup(
    FocusData* focusData,
    const StateMachineListener* listener,
    StateMachineInstance* stateMachineInstance) :
    m_focusData(focusData),
    m_listener(listener),
    m_stateMachineInstance(stateMachineInstance)
{
    // Register ourselves as a listener on the FocusData
    m_focusData->addKeyboardListener(this);
}

KeyboardListenerGroup::~KeyboardListenerGroup()
{
    m_focusData->removeKeyboardListener(this);
}

bool KeyboardListenerGroup::keyInput(Key key,
                                     KeyModifiers modifiers,
                                     bool isPressed,
                                     bool isRepeat)
{
    listener()->performChanges(m_stateMachineInstance, Vec2D(), Vec2D(), 0);
    // Always return false for now. In the future we will let listeners decide
    // whether they stop event propagation
    return false;
}

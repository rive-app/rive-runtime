#include "rive/animation/gamepad_listener_group.hpp"
#include "rive/animation/listener_invocation.hpp"
#include "rive/animation/listener_types/listener_input_type_gamepad.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_listener.hpp"
#include "rive/scripted/scripted_drawable.hpp"
#include "rive/focus_data.hpp"

using namespace rive;

GamepadListenerGroup::GamepadListenerGroup(
    FocusData* focusData,
    const StateMachineListener* listener,
    StateMachineInstance* stateMachineInstance) :
    m_focusData(focusData),
    m_listener(listener),
    m_stateMachineInstance(stateMachineInstance)
{
    m_focusData->addGamepadListener(this);
}

GamepadListenerGroup::~GamepadListenerGroup()
{
    m_focusData->removeGamepadListener(this);
}

bool GamepadListenerGroup::gamepadDispatch(
    const ListenerInvocation& invocation,
    ScriptedDrawable** outDispatchedScriptedDrawable)
{
#ifdef WITH_RIVE_SCRIPTING
    if (m_focusData && m_focusData->parent())
    {

        auto parent = m_focusData->parent();
        if (parent->is<ScriptedDrawable>())
        {
            auto* scriptedDrawable = parent->as<ScriptedDrawable>();
            bool handled = scriptedDrawable->gamepadDispatch(invocation);
            // Report which scripted drawable consumed the focus-tree dispatch
            // so a subsequent broadcast pass can skip it.
            if (outDispatchedScriptedDrawable != nullptr)
            {
                *outDispatchedScriptedDrawable = scriptedDrawable;
            }
            return handled;
        }
    }
#endif
    if (!m_listener ||
        !ListenerInputTypeGamepad::gamepadListenerConstraintsMet(m_listener,
                                                                 invocation))
    {
        return false;
    }
    m_listener->performChanges(m_stateMachineInstance, invocation);
    m_stateMachineInstance->markNeedsAdvance();
    return false;
}

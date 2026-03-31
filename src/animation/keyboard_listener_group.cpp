#include "rive/animation/keyboard_listener_group.hpp"
#include "rive/animation/listener_invocation.hpp"
#include "rive/animation/listener_types/listener_input_type_keyboard.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_listener.hpp"
#include "rive/text/text_input.hpp"
#include "rive/scripted/scripted_drawable.hpp"
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
    if (m_listener)
    {
        if (m_listener->hasListener(ListenerType::keyboard))
        {
            m_focusData->addKeyboardListener(this);
        }
        if (m_listener->hasListener(ListenerType::textInput))
        {
            m_focusData->addTextInputListener(this);
        }
    }
    else
    {
        if (m_focusData && m_focusData->parent())
        {
            auto parent = m_focusData->parent();
            if (parent->is<ScriptedDrawable>())
            {
                auto scriptedObject = ScriptedObject::from(parent);
                if (scriptedObject->wantsKeyboardInput())
                {

                    m_focusData->addKeyboardListener(this);
                }
                if (scriptedObject->wantsTextInput())
                {

                    m_focusData->addTextInputListener(this);
                }
            }
        }
    }
}

KeyboardListenerGroup::~KeyboardListenerGroup()
{
    if (m_listener)
    {

        if (m_listener->hasListener(ListenerType::keyboard))
        {
            m_focusData->removeKeyboardListener(this);
        }
        if (m_listener->hasListener(ListenerType::textInput))
        {
            m_focusData->removeTextInputListener(this);
        }
    }
    else
    {
        if (m_focusData && m_focusData->parent())
        {
            auto parent = m_focusData->parent();
            if (parent->is<ScriptedDrawable>())
            {
                auto scriptedObject = ScriptedObject::from(parent);
                if (scriptedObject->wantsKeyboardInput())
                {

                    m_focusData->removeKeyboardListener(this);
                }
                if (scriptedObject->wantsTextInput())
                {

                    m_focusData->removeTextInputListener(this);
                }
            }
        }
    }
}

bool KeyboardListenerGroup::keyInput(Key key,
                                     KeyModifiers modifiers,
                                     bool isPressed,
                                     bool isRepeat)
{
    // Special case for text inputs.
    // TODO: @hernan consider moving it into a special internal TextInput
    // listener action.
    if (m_focusData && m_focusData->parent())
    {

        auto parent = m_focusData->parent();
        if (parent->is<TextInput>())
        {
            return parent->as<TextInput>()->keyInput(key,
                                                     modifiers,
                                                     isPressed,
                                                     isRepeat);
        }
#ifdef WITH_RIVE_SCRIPTING
        else if (parent->is<ScriptedDrawable>() && !listener())
        {
            auto scriptedObject = ScriptedObject::from(parent);
            if (scriptedObject->wantsKeyboardInput())
            {

                return parent->as<ScriptedDrawable>()->keyInput(key,
                                                                modifiers,
                                                                isPressed,
                                                                isRepeat);
            }
        }
#endif
    }
    if (listener())
    {
        if (!ListenerInputTypeKeyboard::keyboardListenerConstraintsMet(
                listener(),
                key,
                modifiers,
                isPressed,
                isRepeat))
        {
            return false;
        }
        listener()->performChanges(
            m_stateMachineInstance,
            ListenerInvocation::keyboard(key, modifiers, isPressed, isRepeat));
        // Always return false for now. In the future we will let listeners
        // decide whether they stop event propagation
    }
    return false;
}

bool KeyboardListenerGroup::textInput(const std::string& text)
{

    // Special case for text inputs.
    // TODO: @hernan consider moving it into a special internal TextInput
    // listener action.
    if (m_focusData && m_focusData->parent())
    {

        auto parent = m_focusData->parent();
        if (parent->is<TextInput>())
        {
            return parent->as<TextInput>()->textInput(text);
        }
#ifdef WITH_RIVE_SCRIPTING
        else if (parent->is<ScriptedDrawable>() && !listener())
        {
            auto scriptedObject = ScriptedObject::from(parent);
            if (scriptedObject->wantsTextInput())
            {
                return parent->as<ScriptedDrawable>()->textInput(text);
            }
        }
#endif
    }
    if (listener())
    {
        listener()->performChanges(m_stateMachineInstance,
                                   ListenerInvocation::textInput(text));
    }
    return false;
}
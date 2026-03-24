#include "rive/animation/listener_types/listener_input_type_keyboard.hpp"
#include "rive/animation/state_machine_listener.hpp"
#include "rive/inputs/keyboard_input.hpp"
#include "rive/rive_types.hpp"

#include <algorithm>

using namespace rive;

const KeyboardInput* ListenerInputTypeKeyboard::keyboardInput(
    size_t index) const
{
    if (index < m_keyboardInputs.size())
    {
        return m_keyboardInputs[index];
    }
    return nullptr;
}

void ListenerInputTypeKeyboard::addKeyboardInput(KeyboardInput* input)
{
    if (input == nullptr)
    {
        return;
    }
    for (KeyboardInput* existing : m_keyboardInputs)
    {
        if (existing == input)
        {
            return;
        }
    }
    m_keyboardInputs.push_back(input);
}

bool ListenerInputTypeKeyboard::keyPhaseMatches(uint32_t keyPhase,
                                                bool isPressed,
                                                bool isRepeat)
{
    const uint32_t mask = keyPhase & KeyboardKeyPhaseMask::all;
    if (mask == 0)
    {
        return false;
    }
    if (isPressed && isRepeat)
    {
        return (mask & KeyboardKeyPhaseMask::repeat) != 0;
    }
    if (isPressed)
    {
        return (mask & KeyboardKeyPhaseMask::down) != 0;
    }
    return (mask & KeyboardKeyPhaseMask::up) != 0;
}

bool ListenerInputTypeKeyboard::keyboardInputMatches(const KeyboardInput& input,
                                                     Key key,
                                                     KeyModifiers modifiers,
                                                     bool isPressed,
                                                     bool isRepeat)
{
    const uint32_t keyValue = static_cast<uint32_t>(key);
    if (input.keyType() != static_cast<uint32_t>(-1) &&
        input.keyType() != keyValue)
    {
        return false;
    }
    if (input.modifiers() != static_cast<uint32_t>(modifiers))
    {
        return false;
    }
    return keyPhaseMatches(input.keyPhase(), isPressed, isRepeat);
}

bool ListenerInputTypeKeyboard::keyboardListenerConstraintsMet(
    const StateMachineListener* listener,
    Key key,
    KeyModifiers modifiers,
    bool isPressed,
    bool isRepeat)
{
    if (listener == nullptr)
    {
        return false;
    }
    for (size_t i = 0; i < listener->listenerInputTypeCount(); ++i)
    {
        const ListenerInputType* lit = listener->listenerInputType(i);
        if (lit == nullptr || !lit->is<ListenerInputTypeKeyboard>())
        {
            continue;
        }
        const ListenerInputTypeKeyboard* litk =
            lit->as<ListenerInputTypeKeyboard>();
        if (litk->keyboardInputCount() == 0)
        {
            return true;
        }
        bool anyMatches = false;
        for (size_t j = 0; j < litk->keyboardInputCount(); ++j)
        {
            const KeyboardInput* ki = litk->keyboardInput(j);
            if (ki != nullptr &&
                keyboardInputMatches(*ki, key, modifiers, isPressed, isRepeat))
            {
                anyMatches = true;
                break;
            }
        }
        if (anyMatches)
        {
            return true;
        }
    }
    return false;
}

#include "rive/animation/listener_types/listener_input_type_gamepad.hpp"
#include "rive/animation/listener_invocation.hpp"
#include "rive/animation/state_machine_listener.hpp"
#include "rive/input/gamepad_snapshot.hpp"
#include "rive/input/standard_gamepad.hpp"
#include "rive/inputs/gamepad_button_phase.hpp"
#include "rive/inputs/gamepad_input.hpp"
#include "rive/inputs/gamepad_input_kind.hpp"

using namespace rive;

const GamepadInput* ListenerInputTypeGamepad::gamepadInput(size_t index) const
{
    if (index < m_gamepadInputs.size())
    {
        return m_gamepadInputs[index];
    }
    return nullptr;
}

void ListenerInputTypeGamepad::addGamepadInput(GamepadInput* input)
{
    if (input == nullptr)
    {
        return;
    }
    for (GamepadInput* existing : m_gamepadInputs)
    {
        if (existing == input)
        {
            return;
        }
    }
    m_gamepadInputs.push_back(input);
}

bool ListenerInputTypeGamepad::gamepadButtonPhaseMatches(uint32_t phase,
                                                         bool isPressed)
{
    const uint32_t mask = phase & GamepadButtonPhaseMask::all;
    if (mask == 0)
    {
        return false;
    }
    if (isPressed)
    {
        return (mask & GamepadButtonPhaseMask::down) != 0;
    }
    return (mask & GamepadButtonPhaseMask::up) != 0;
}

bool ListenerInputTypeGamepad::gamepadInputMatches(
    const GamepadInput& input,
    const ListenerInvocation& invocation)
{
    const auto kind = static_cast<GamepadInputKind>(input.kind());
    switch (kind)
    {
        case GamepadInputKind::connected:
            return invocation.asGamepadConnected() != nullptr;
        case GamepadInputKind::disconnected:
            return invocation.asGamepadDisconnected() != nullptr;
        case GamepadInputKind::button:
        case GamepadInputKind::axis:
        {
            const GamepadEventInvocation* ev = invocation.asGamepadEvent();
            if (ev == nullptr)
            {
                return false;
            }
            const bool wantButton = kind == GamepadInputKind::button;
            const auto expected = wantButton ? GamepadInputChangeKind::button
                                             : GamepadInputChangeKind::axis;
            if (ev->change.kind != expected)
            {
                return false;
            }

            const auto mapping =
                static_cast<GamepadInputMapping>(input.mapping());
            if (mapping == GamepadInputMapping::standard)
            {
                if (wantButton)
                {
                    if (!ev->hasStandardButtonIntent ||
                        static_cast<uint32_t>(ev->standardButton) !=
                            input.inputIndex())
                    {
                        return false;
                    }
                }
                else
                {
                    if (!ev->hasStandardAxisIntent ||
                        static_cast<uint32_t>(ev->standardAxis) !=
                            input.inputIndex())
                    {
                        return false;
                    }
                }
            }
            else
            {
                if (static_cast<uint32_t>(ev->change.index) !=
                    input.inputIndex())
                {
                    return false;
                }
            }

            if (wantButton)
            {
                const bool isPressed = ev->change.value >= 0.5f;
                return gamepadButtonPhaseMatches(input.buttonPhase(),
                                                 isPressed);
            }
            return true;
        }
    }
    return false;
}

bool ListenerInputTypeGamepad::gamepadListenerConstraintsMet(
    const StateMachineListener* listener,
    const ListenerInvocation& invocation)
{
    if (listener == nullptr)
    {
        return false;
    }
    for (size_t i = 0; i < listener->listenerInputTypeCount(); ++i)
    {
        const ListenerInputType* lit = listener->listenerInputType(i);
        if (lit == nullptr || !lit->is<ListenerInputTypeGamepad>())
        {
            continue;
        }
        const ListenerInputTypeGamepad* litg =
            lit->as<ListenerInputTypeGamepad>();
        if (litg->gamepadInputCount() == 0)
        {
            return true;
        }
        bool anyMatches = false;
        for (size_t j = 0; j < litg->gamepadInputCount(); ++j)
        {
            const GamepadInput* gi = litg->gamepadInput(j);
            if (gi != nullptr && gamepadInputMatches(*gi, invocation))
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

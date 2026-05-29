#ifndef _RIVE_LISTENER_INPUT_TYPE_GAMEPAD_HPP_
#define _RIVE_LISTENER_INPUT_TYPE_GAMEPAD_HPP_

#include "rive/generated/animation/listener_types/listener_input_type_gamepad_base.hpp"

#include <cstdint>
#include <vector>

namespace rive
{

class GamepadInput;
class ListenerInvocation;
class StateMachineListener;

class ListenerInputTypeGamepad : public ListenerInputTypeGamepadBase
{
public:
    size_t gamepadInputCount() const { return m_gamepadInputs.size(); }
    const GamepadInput* gamepadInput(size_t index) const;
    void addGamepadInput(GamepadInput* input);

    /// True if `phase` (a `GamepadButtonPhaseMask`) opted in to the current
    /// press / release transition.
    static bool gamepadButtonPhaseMatches(uint32_t phase, bool isPressed);

    /// True if `input` selects the field/event represented by `invocation`.
    static bool gamepadInputMatches(const GamepadInput& input,
                                    const ListenerInvocation& invocation);

    /// True if any `ListenerInputTypeGamepad` on `listener` matches the
    /// invocation. An empty input list on any `ListenerInputTypeGamepad`
    /// matches any gamepad event (catch-all, preserves legacy behavior).
    static bool gamepadListenerConstraintsMet(
        const StateMachineListener* listener,
        const ListenerInvocation& invocation);

private:
    std::vector<GamepadInput*> m_gamepadInputs;
};

} // namespace rive

#endif

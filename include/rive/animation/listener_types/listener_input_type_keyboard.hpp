#ifndef _RIVE_LISTENER_INPUT_TYPE_KEYBOARD_HPP_
#define _RIVE_LISTENER_INPUT_TYPE_KEYBOARD_HPP_

#include "rive/generated/animation/listener_types/listener_input_type_keyboard_base.hpp"
#include "rive/input/focusable.hpp"

#include <vector>

namespace rive
{
class KeyboardInput;
class StateMachineListener;

class ListenerInputTypeKeyboard : public ListenerInputTypeKeyboardBase
{
public:
    size_t keyboardInputCount() const { return m_keyboardInputs.size(); }
    const KeyboardInput* keyboardInput(size_t index) const;
    void addKeyboardInput(KeyboardInput* input);

    static bool keyPhaseMatches(uint32_t keyPhase,
                                bool isPressed,
                                bool isRepeat);
    static bool keyboardInputMatches(const KeyboardInput& input,
                                     Key key,
                                     KeyModifiers modifiers,
                                     bool isPressed,
                                     bool isRepeat);
    static bool keyboardListenerConstraintsMet(
        const StateMachineListener* listener,
        Key key,
        KeyModifiers modifiers,
        bool isPressed,
        bool isRepeat);

private:
    std::vector<KeyboardInput*> m_keyboardInputs;
};
} // namespace rive

#endif

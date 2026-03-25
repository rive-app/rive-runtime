#ifndef _RIVE_KEYBOARD_LISTENER_GROUP_HPP_
#define _RIVE_KEYBOARD_LISTENER_GROUP_HPP_

#include "rive/input/keyboard_listener.hpp"
#include "rive/listener_type.hpp"

namespace rive
{
class FocusData;
class StateMachineListener;
class StateMachineInstance;

/// A KeyboardListenerGroup manages a keyboard listener that's attached
/// to a FocusData. When a key is pressed, the focus manager will call onKey
/// only on the focused one.
class KeyboardListenerGroup : public KeyboardListener
{
public:
    KeyboardListenerGroup(FocusData* focusData,
                          const StateMachineListener* listener,
                          StateMachineInstance* stateMachineInstance);
    ~KeyboardListenerGroup();

    /// Get the listener this group is managing.
    const StateMachineListener* listener() const { return m_listener; }

    /// Get the FocusData this group is attached to.
    FocusData* focusData() const { return m_focusData; }

    /// Called when the associated FocusData receives a key input.
    bool keyInput(Key key,
                  KeyModifiers modifiers,
                  bool isPressed,
                  bool isRepeat) override;

    /// Called when the associated FocusData receives text input.
    bool textInput(const std::string& text) override;

private:
    FocusData* m_focusData;
    const StateMachineListener* m_listener;
    StateMachineInstance* m_stateMachineInstance;
};

} // namespace rive

#endif

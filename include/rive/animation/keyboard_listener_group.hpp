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
  KeyboardListenerGroup(
      FocusData* focusData,
      const StateMachineListener* listener,
      StateMachineInstance* stateMachineInstance);
  ~KeyboardListenerGroup();

  /// Get the listener this group is managing.
  const StateMachineListener* listener() const { return m_listener; }

  /// Get the FocusData this group is attached to.
  FocusData* focusData() const { return m_focusData; }

  /// Called when the associated FocusData receives a key input.
  bool keyInput(Key key, KeyModifiers modifiers, bool isPressed, bool isRepeat)
      override;

  /// Called in the capture phase, for a capture-phase listener. Its actions
  /// run on the key phases it asks for; it claims every phase of a key it
  /// names exactly, so its own press and release both stay away from the
  /// target, and any key its action hit().
  bool captureKeyInput(
      Key key,
      KeyModifiers modifiers,
      bool isPressed,
      bool isRepeat) override;

  /// Called when the associated FocusData receives text input. Claims it
  /// when the listener's action hit() it.
  bool textInput(const std::string& text) override;

  /// Called in the capture phase of text input, for a capture-phase
  /// listener. Claims the text when its action hit() it.
  bool captureTextInput(const std::string& text) override;

private:
  FocusData* m_focusData;
  const StateMachineListener* m_listener;
  StateMachineInstance* m_stateMachineInstance;
  /// Whether the listener sits in the capture phase, for keys and text.
  bool m_isCapture;
};

} // namespace rive

#endif

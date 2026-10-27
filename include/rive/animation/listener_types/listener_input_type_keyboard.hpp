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
#ifdef WITH_RIVE_EDITOR
  void clearKeyboardInputsForEditor() { m_keyboardInputs.clear(); }
#endif

  static bool keyPhaseMatches(uint32_t keyPhase, bool isPressed, bool isRepeat);
  static bool keyboardInputMatches(
      const KeyboardInput& input,
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
  /// Whether the listener names this key exactly: one of its keyboard
  /// inputs filters for this key (not 'any') with exactly these modifiers.
  /// The key phase is ignored, so a listener that owns a key owns its
  /// press, repeats and release alike. A listener with no keyboard input,
  /// or only 'any' ones, owns nothing.
  static bool keyboardListenerOwnsKey(
      const StateMachineListener* listener,
      Key key,
      KeyModifiers modifiers);

private:
  std::vector<KeyboardInput*> m_keyboardInputs;
};
} // namespace rive

#endif

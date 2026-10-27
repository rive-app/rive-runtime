#ifndef _RIVE_KEYBOARD_LISTENER_HPP_
#define _RIVE_KEYBOARD_LISTENER_HPP_
#include "rive/input/focusable.hpp"

namespace rive
{

/// Interface for objects that want to be notified of key inputs on a
/// FocusData.
class KeyboardListener
{
public:
  virtual ~KeyboardListener() = default;

  /// Called when the associated FocusData receives key inputs.
  virtual bool keyInput(
      Key key,
      KeyModifiers modifiers,
      bool isPressed,
      bool isRepeat) = 0;

  /// Called when the associated FocusData receives text input.
  virtual bool textInput(const std::string& text) = 0;

  /// Called in the capture phase of a key event, for listeners registered
  /// with FocusData::addCaptureKeyboardListener. Returning true claims the
  /// key from everything deeper and from the whole bubble.
  virtual bool captureKeyInput(
      Key key,
      KeyModifiers modifiers,
      bool isPressed,
      bool isRepeat)
  {
    return false;
  }
};

} // namespace rive

#endif

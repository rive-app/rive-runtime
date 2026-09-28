#ifndef _RIVE_LISTENER_INPUT_VALUE_HPP_
#define _RIVE_LISTENER_INPUT_VALUE_HPP_

#include <cstdint>

namespace rive
{
/// Values for \c ListenerViewModelChangeBase::inputValue() — which value of the
/// invocation that fired the listener is written to the view model property,
/// instead of the action's own bindable value. The value is written into the
/// action's bindable property, which has the value's type; its data bind
/// carries it to the view model property, through its converter when the
/// view model property has another type. When the invocation does not carry
/// the value (e.g. `pointerX` on a gamepad event) or the bindable property is
/// not of the value's type, the action is skipped.
enum class ListenerInputValue : uint8_t
{
    /// Write the bindable property's value (a constant, or another view model
    /// property bound to it).
    none = 0,
    /// Pointer position, in the state machine artboard's space (number).
    pointerX = 1,
    pointerY = 2,
    /// Pointer movement since the previous event, `position - previousPosition`
    /// (number).
    pointerDeltaX = 3,
    pointerDeltaY = 4,
    /// Whether the key is down: true on press and repeat, false on release
    /// (boolean).
    keyPressed = 5,
    /// Committed text of a text input event (string).
    text = 6,
    /// True on focus, false on blur (boolean).
    focused = 7,
    /// Whether W3C button `inputValueIndex` is pressed (boolean).
    gamepadButtonPressed = 8,
    /// Analog value (0-1) of W3C button `inputValueIndex` (number).
    gamepadButtonValue = 9,
    /// Value of W3C axis `inputValueIndex` (number).
    gamepadAxis = 10,
    /// Value of whichever button or axis changed (number).
    gamepadChangedValue = 11,
};
} // namespace rive

#endif

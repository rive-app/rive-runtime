#ifndef _RIVE_STANDARD_GAMEPAD_HPP_
#define _RIVE_STANDARD_GAMEPAD_HPP_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rive
{

/// Logical face / control positions for the W3C Standard Gamepad layout.
/// Indices match https://w3c.github.io/gamepad/#remapping button indices.
enum class StandardGamepadButton : uint8_t
{
    south = 0x00,
    east = 0x01,
    west = 0x02,
    north = 0x03,
    leftShoulder = 0x04,
    rightShoulder = 0x05,
    leftTrigger = 0x06,
    rightTrigger = 0x07,
    back = 0x08,
    forward = 0x09,
    leftStick = 0x0a,
    rightStick = 0x0b,
    dpadUp = 0x0c,
    dpadDown = 0x0d,
    dpadLeft = 0x0e,
    dpadRight = 0x0f,
    start = 0x10,
};

/// Standard axis slots for the W3C layout (indices into `GamepadSnapshot::axes`
/// when that vector is standard-mapped; missing indices read as 0). Analog
/// button pressure uses `GamepadSnapshot::buttonValues` at the same indices as
/// `StandardGamepadButton`.
enum class StandardGamepadAxis : uint8_t
{
    leftX = 0,
    leftY = 1,
    rightX = 2,
    rightY = 3,
    leftTrigger = 4,
    rightTrigger = 5,
};

inline float standardGamepadAxisValue(const std::vector<float>& axes,
                                      StandardGamepadAxis axis)
{
    const size_t i = static_cast<size_t>(axis);
    return i < axes.size() ? axes[i] : 0.f;
}

inline size_t standardGamepadButtonIndex(StandardGamepadButton b)
{
    return static_cast<size_t>(b);
}

inline bool standardGamepadButtonPressed(uint64_t buttonMask,
                                         StandardGamepadButton button)
{
    const uint64_t bit = uint64_t(1) << standardGamepadButtonIndex(button);
    return (buttonMask & bit) != 0;
}

} // namespace rive

#endif

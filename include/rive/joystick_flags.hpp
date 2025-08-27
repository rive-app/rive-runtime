#ifndef _RIVE_JOYSTICK_FLAGS_HPP_
#define _RIVE_JOYSTICK_FLAGS_HPP_

#include "rive/enum_bitset.hpp"

namespace rive
{
enum class JoystickFlags : unsigned char
{
    /// Whether to invert the application of the x axis.
    invertX = 1 << 0,

    /// Whether to invert the application of the y axis.
    invertY = 1 << 1,

    /// Whether this Joystick works in world space.
    worldSpace = 1 << 2
};
RIVE_MAKE_ENUM_BITSET(JoystickFlags)
} // namespace rive
#endif

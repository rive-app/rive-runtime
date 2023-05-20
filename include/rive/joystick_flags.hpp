#ifndef _RIVE_JOYSTICK_FLAGS_HPP_
#define _RIVE_JOYSTICK_FLAGS_HPP_
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

inline constexpr JoystickFlags operator&(JoystickFlags lhs, JoystickFlags rhs)
{
    return static_cast<JoystickFlags>(static_cast<std::underlying_type<JoystickFlags>::type>(lhs) &
                                      static_cast<std::underlying_type<JoystickFlags>::type>(rhs));
}

inline constexpr JoystickFlags operator^(JoystickFlags lhs, JoystickFlags rhs)
{
    return static_cast<JoystickFlags>(static_cast<std::underlying_type<JoystickFlags>::type>(lhs) ^
                                      static_cast<std::underlying_type<JoystickFlags>::type>(rhs));
}

inline constexpr JoystickFlags operator|(JoystickFlags lhs, JoystickFlags rhs)
{
    return static_cast<JoystickFlags>(static_cast<std::underlying_type<JoystickFlags>::type>(lhs) |
                                      static_cast<std::underlying_type<JoystickFlags>::type>(rhs));
}

inline constexpr JoystickFlags operator~(JoystickFlags rhs)
{
    return static_cast<JoystickFlags>(~static_cast<std::underlying_type<JoystickFlags>::type>(rhs));
}

inline JoystickFlags& operator|=(JoystickFlags& lhs, JoystickFlags rhs)
{
    lhs = static_cast<JoystickFlags>(static_cast<std::underlying_type<JoystickFlags>::type>(lhs) |
                                     static_cast<std::underlying_type<JoystickFlags>::type>(rhs));

    return lhs;
}

inline JoystickFlags& operator&=(JoystickFlags& lhs, JoystickFlags rhs)
{
    lhs = static_cast<JoystickFlags>(static_cast<std::underlying_type<JoystickFlags>::type>(lhs) &
                                     static_cast<std::underlying_type<JoystickFlags>::type>(rhs));

    return lhs;
}

inline JoystickFlags& operator^=(JoystickFlags& lhs, JoystickFlags rhs)
{
    lhs = static_cast<JoystickFlags>(static_cast<std::underlying_type<JoystickFlags>::type>(lhs) ^
                                     static_cast<std::underlying_type<JoystickFlags>::type>(rhs));

    return lhs;
}
} // namespace rive
#endif
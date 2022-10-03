#ifndef _RIVE_STATE_TRANSITION_FLAGS_HPP_
#define _RIVE_STATE_TRANSITION_FLAGS_HPP_

#include <type_traits>

namespace rive
{
enum class StateTransitionFlags : unsigned char
{
    None = 0,

    /// Whether the transition is disabled.
    Disabled = 1 << 0,

    /// Whether the transition duration is a percentage or time in ms.
    DurationIsPercentage = 1 << 1,

    /// Whether exit time is enabled.
    EnableExitTime = 1 << 2,

    /// Whether the exit time is a percentage or time in ms.
    ExitTimeIsPercentage = 1 << 3,

    /// Whether the animation is held at exit or if it keeps advancing
    /// during mixing.
    PauseOnExit = 1 << 4

};

inline constexpr StateTransitionFlags operator&(StateTransitionFlags lhs, StateTransitionFlags rhs)
{
    return static_cast<StateTransitionFlags>(
        static_cast<std::underlying_type<StateTransitionFlags>::type>(lhs) &
        static_cast<std::underlying_type<StateTransitionFlags>::type>(rhs));
}

inline constexpr StateTransitionFlags operator^(StateTransitionFlags lhs, StateTransitionFlags rhs)
{
    return static_cast<StateTransitionFlags>(
        static_cast<std::underlying_type<StateTransitionFlags>::type>(lhs) ^
        static_cast<std::underlying_type<StateTransitionFlags>::type>(rhs));
}

inline constexpr StateTransitionFlags operator|(StateTransitionFlags lhs, StateTransitionFlags rhs)
{
    return static_cast<StateTransitionFlags>(
        static_cast<std::underlying_type<StateTransitionFlags>::type>(lhs) |
        static_cast<std::underlying_type<StateTransitionFlags>::type>(rhs));
}

inline constexpr StateTransitionFlags operator~(StateTransitionFlags rhs)
{
    return static_cast<StateTransitionFlags>(
        ~static_cast<std::underlying_type<StateTransitionFlags>::type>(rhs));
}

inline StateTransitionFlags& operator|=(StateTransitionFlags& lhs, StateTransitionFlags rhs)
{
    lhs = static_cast<StateTransitionFlags>(
        static_cast<std::underlying_type<StateTransitionFlags>::type>(lhs) |
        static_cast<std::underlying_type<StateTransitionFlags>::type>(rhs));

    return lhs;
}

inline StateTransitionFlags& operator&=(StateTransitionFlags& lhs, StateTransitionFlags rhs)
{
    lhs = static_cast<StateTransitionFlags>(
        static_cast<std::underlying_type<StateTransitionFlags>::type>(lhs) &
        static_cast<std::underlying_type<StateTransitionFlags>::type>(rhs));

    return lhs;
}

inline StateTransitionFlags& operator^=(StateTransitionFlags& lhs, StateTransitionFlags rhs)
{
    lhs = static_cast<StateTransitionFlags>(
        static_cast<std::underlying_type<StateTransitionFlags>::type>(lhs) ^
        static_cast<std::underlying_type<StateTransitionFlags>::type>(rhs));

    return lhs;
}
} // namespace rive
#endif
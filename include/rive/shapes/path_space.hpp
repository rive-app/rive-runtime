#ifndef _RIVE_PATH_SPACE_HPP_
#define _RIVE_PATH_SPACE_HPP_

#include "rive/rive_types.hpp"

namespace rive
{
enum class PathSpace : unsigned char
{
    Neither = 0,
    Local = 1 << 1,
    World = 1 << 2,
    Clipping = 1 << 3,
    FollowPath = 1 << 4
};

inline constexpr PathSpace operator&(PathSpace lhs, PathSpace rhs)
{
    return static_cast<PathSpace>(static_cast<std::underlying_type<PathSpace>::type>(lhs) &
                                  static_cast<std::underlying_type<PathSpace>::type>(rhs));
}

inline constexpr PathSpace operator^(PathSpace lhs, PathSpace rhs)
{
    return static_cast<PathSpace>(static_cast<std::underlying_type<PathSpace>::type>(lhs) ^
                                  static_cast<std::underlying_type<PathSpace>::type>(rhs));
}

inline constexpr PathSpace operator|(PathSpace lhs, PathSpace rhs)
{
    return static_cast<PathSpace>(static_cast<std::underlying_type<PathSpace>::type>(lhs) |
                                  static_cast<std::underlying_type<PathSpace>::type>(rhs));
}

inline constexpr PathSpace operator~(PathSpace rhs)
{
    return static_cast<PathSpace>(~static_cast<std::underlying_type<PathSpace>::type>(rhs));
}

inline PathSpace& operator|=(PathSpace& lhs, PathSpace rhs)
{
    lhs = static_cast<PathSpace>(static_cast<std::underlying_type<PathSpace>::type>(lhs) |
                                 static_cast<std::underlying_type<PathSpace>::type>(rhs));

    return lhs;
}

inline PathSpace& operator&=(PathSpace& lhs, PathSpace rhs)
{
    lhs = static_cast<PathSpace>(static_cast<std::underlying_type<PathSpace>::type>(lhs) &
                                 static_cast<std::underlying_type<PathSpace>::type>(rhs));

    return lhs;
}

inline PathSpace& operator^=(PathSpace& lhs, PathSpace rhs)
{
    lhs = static_cast<PathSpace>(static_cast<std::underlying_type<PathSpace>::type>(lhs) ^
                                 static_cast<std::underlying_type<PathSpace>::type>(rhs));

    return lhs;
}
} // namespace rive

#endif

#ifndef _RIVE_PATH_FLAGS_HPP_
#define _RIVE_PATH_FLAGS_HPP_

#include "rive/rive_types.hpp"

namespace rive
{
enum class PathFlags : uint8_t
{
    none = 0,
    local = 1 << 1,
    world = 1 << 2,
    clipping = 1 << 3,
    followPath = 1 << 4,
    neverDeferUpdate = 1 << 5,
};

inline constexpr PathFlags operator&(PathFlags lhs, PathFlags rhs)
{
    return static_cast<PathFlags>(static_cast<std::underlying_type<PathFlags>::type>(lhs) &
                                  static_cast<std::underlying_type<PathFlags>::type>(rhs));
}

inline constexpr PathFlags operator^(PathFlags lhs, PathFlags rhs)
{
    return static_cast<PathFlags>(static_cast<std::underlying_type<PathFlags>::type>(lhs) ^
                                  static_cast<std::underlying_type<PathFlags>::type>(rhs));
}

inline constexpr PathFlags operator|(PathFlags lhs, PathFlags rhs)
{
    return static_cast<PathFlags>(static_cast<std::underlying_type<PathFlags>::type>(lhs) |
                                  static_cast<std::underlying_type<PathFlags>::type>(rhs));
}

inline constexpr PathFlags operator~(PathFlags rhs)
{
    return static_cast<PathFlags>(~static_cast<std::underlying_type<PathFlags>::type>(rhs));
}

inline PathFlags& operator|=(PathFlags& lhs, PathFlags rhs)
{
    lhs = static_cast<PathFlags>(static_cast<std::underlying_type<PathFlags>::type>(lhs) |
                                 static_cast<std::underlying_type<PathFlags>::type>(rhs));

    return lhs;
}

inline PathFlags& operator&=(PathFlags& lhs, PathFlags rhs)
{
    lhs = static_cast<PathFlags>(static_cast<std::underlying_type<PathFlags>::type>(lhs) &
                                 static_cast<std::underlying_type<PathFlags>::type>(rhs));

    return lhs;
}

inline PathFlags& operator^=(PathFlags& lhs, PathFlags rhs)
{
    lhs = static_cast<PathFlags>(static_cast<std::underlying_type<PathFlags>::type>(lhs) ^
                                 static_cast<std::underlying_type<PathFlags>::type>(rhs));

    return lhs;
}
} // namespace rive

#endif

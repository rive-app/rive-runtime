#ifndef _RIVE_SHAPE_PATH_FLAGS_HPP_
#define _RIVE_SHAPE_PATH_FLAGS_HPP_

#include "rive/rive_types.hpp"

namespace rive
{
enum class ShapePathFlags : uint8_t
{
    none = 0,
    hidden = 1 << 0, // Unused at runtime
    isCounterClockwise = 1 << 1,
};

RIVE_MAKE_ENUM_BITSET(ShapePathFlags)
} // namespace rive

#endif

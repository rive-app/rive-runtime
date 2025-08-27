#ifndef _RIVE_ADVANCE_FLAGS_HPP_
#define _RIVE_ADVANCE_FLAGS_HPP_

#include "rive/enum_bitset.hpp"

namespace rive
{
enum class AdvanceFlags : unsigned short
{
    None = 0,

    /// Whether NestedArtboards should advance
    AdvanceNested = 1 << 0,

    /// Whether the Component should animate when advancing
    Animate = 1 << 1,

    /// Whether this Component is on the root artboard
    IsRoot = 1 << 2,

    /// Whether we are advancing to a new frame
    NewFrame = 1 << 3,
};
RIVE_MAKE_ENUM_BITSET(AdvanceFlags)
} // namespace rive
#endif

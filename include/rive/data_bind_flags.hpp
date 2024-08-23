#ifndef _RIVE_DATA_BIND_FLAGS_HPP_
#define _RIVE_DATA_BIND_FLAGS_HPP_

#include "rive/enum_bitset.hpp"

namespace rive
{
enum class DataBindFlags : unsigned short
{
    /// Whether the main binding direction is to source (0) or to target (1)
    Direction = 1 << 0,

    /// Whether the binding direction is twoWay
    TwoWay = 1 << 1,

    /// Whether the binding happens only once
    Once = 1 << 2,

    /// Flag if set to target
    ToTarget = 0,

    /// Flag if set to source
    ToSource = 1 << 0,

};

RIVE_MAKE_ENUM_BITSET(DataBindFlags)
} // namespace rive
#endif

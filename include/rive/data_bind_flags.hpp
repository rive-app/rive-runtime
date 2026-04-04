#ifndef _RIVE_DATA_BIND_FLAGS_HPP_
#define _RIVE_DATA_BIND_FLAGS_HPP_

#include "rive/enums.hpp"

namespace rive
{
enum class DataBindFlags : unsigned short
{
    None = 0,

    /// Whether the main binding direction is to source (0) or to target (1)
    Direction = 1 << 0,

    /// Whether the binding direction is twoWay
    TwoWay = 1 << 1,

    /// Whether the binding happens only once
    Once = 1 << 2,

    /// Whether source to target runs before target to source
    SourceToTargetRunsFirst = 1 << 3,

    /// Whether source to target runs before target to source
    NameBased = 1 << 4,

    /// Flag if set to target
    ToTarget = 0,

    /// Flag if set to source
    ToSource = 1 << 0,

};
} // namespace rive
#endif

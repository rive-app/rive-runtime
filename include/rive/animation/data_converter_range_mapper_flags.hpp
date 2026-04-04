#ifndef _RIVE_DATA_CONVERTER_RANGE_MAPPER_FLAGS_HPP_
#define _RIVE_DATA_CONVERTER_RANGE_MAPPER_FLAGS_HPP_

#include "rive/enums.hpp"

namespace rive
{
enum class DataConverterRangeMapperFlags : unsigned short
{
    None = 0,

    /// Whether the lower bound should be clamped
    ClampLower = 1 << 0,

    /// Whether the upper bound should be clamped
    ClampUpper = 1 << 1,

    /// Whether the value should wrap if it exceeds the range
    Modulo = 1 << 2,

    /// Whether to reverse the mapping
    Reverse = 1 << 3,

};
} // namespace rive
#endif
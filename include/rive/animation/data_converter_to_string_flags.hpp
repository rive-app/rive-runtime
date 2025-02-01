#ifndef _RIVE_DATA_CONVERTER_TO_STRING_FLAGS_HPP_
#define _RIVE_DATA_CONVERTER_TO_STRING_FLAGS_HPP_

#include "rive/enum_bitset.hpp"

namespace rive
{
enum class DataConverterToStringFlags : unsigned short
{

    /// Whether to round to decimals
    Round = 1 << 0,

    /// Whether to remove trailing zeros
    TrailingZeros = 1 << 1,

};

RIVE_MAKE_ENUM_BITSET(DataConverterToStringFlags)
} // namespace rive
#endif
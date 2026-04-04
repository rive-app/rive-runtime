#ifndef _RIVE_DATA_CONVERTER_TO_STRING_FLAGS_HPP_
#define _RIVE_DATA_CONVERTER_TO_STRING_FLAGS_HPP_

#include "rive/enums.hpp"

namespace rive
{
enum class DataConverterToStringFlags : unsigned short
{
    None = 0,

    /// Whether to round to decimals
    Round = 1 << 0,

    /// Whether to remove trailing zeros
    TrailingZeros = 1 << 1,

    /// Whether to format the number with commans
    FormatWithCommas = 1 << 2,

};
} // namespace rive
#endif
#ifndef _RIVE_CORE_BYTES_TYPE_HPP_
#define _RIVE_CORE_BYTES_TYPE_HPP_

#include "rive/span.hpp"
#include <cstdint>

namespace rive
{
class BinaryReader;
class CoreBytesType
{
public:
    static const int id = 1;
    static Span<const uint8_t> deserialize(BinaryReader& reader);
};
} // namespace rive
#endif
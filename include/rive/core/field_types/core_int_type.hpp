#ifndef _RIVE_CORE_INT_TYPE_HPP_
#define _RIVE_CORE_INT_TYPE_HPP_

#include "rive/core/field_types/core_uint_type.hpp"
#include <cstdint>

namespace rive
{
class BinaryReader;

/// A signed integer, zigzag encoded so small negatives stay one byte.
///
/// It deliberately shares CoreUintType's field id: the ToC only has two bits
/// per property, and on the wire this *is* a varuint. A runtime that doesn't
/// know the property still skips exactly the right number of bytes.
class CoreIntType
{
public:
    static const int id = CoreUintType::id;
    static int32_t deserialize(BinaryReader& reader);
#ifdef WITH_RIVE_TOOLS
    static int32_t deserializeRev(BinaryReader& reader);
#endif

    /// (n << 1) ^ (n >> 31) — maps small magnitudes of either sign onto small
    /// unsigned values so the varuint stays short.
    static uint32_t zigzagEncode(int32_t value)
    {
        return (static_cast<uint32_t>(value) << 1) ^
               static_cast<uint32_t>(value >> 31);
    }

    static int32_t zigzagDecode(uint32_t value)
    {
        return static_cast<int32_t>(value >> 1) ^
               -static_cast<int32_t>(value & 1);
    }
};
} // namespace rive
#endif

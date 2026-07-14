#ifndef _RIVE_CORE_UINT64_TYPE_HPP_
#define _RIVE_CORE_UINT64_TYPE_HPP_

#include <cstdint>

namespace rive
{
class BinaryReader;
class CoreUint64Type
{
public:
    // Shares the uint id, both are LEB128 varints on the wire.
    static const int id = 0;
    static uint64_t deserialize(BinaryReader& reader);
#ifdef WITH_RIVE_TOOLS
    static uint64_t deserializeRev(BinaryReader& reader);
#endif
};
} // namespace rive
#endif

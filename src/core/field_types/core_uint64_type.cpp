#include "rive/core/field_types/core_uint64_type.hpp"
#include "rive/core/binary_reader.hpp"

using namespace rive;

uint64_t CoreUint64Type::deserialize(BinaryReader& reader)
{
    return reader.readVarUint64();
}

#ifdef WITH_RIVE_TOOLS
uint64_t CoreUint64Type::deserializeRev(BinaryReader& reader)
{
    return deserialize(reader);
}
#endif

#include "rive/core/field_types/core_int_type.hpp"
#include "rive/core/binary_reader.hpp"

using namespace rive;

int32_t CoreIntType::deserialize(BinaryReader& reader)
{
    return zigzagDecode(reader.readVarUintAs<uint32_t>());
}

#ifdef WITH_RIVE_TOOLS
int32_t CoreIntType::deserializeRev(BinaryReader& reader)
{
    return deserialize(reader);
}
#endif

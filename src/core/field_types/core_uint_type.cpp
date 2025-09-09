#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/core/binary_reader.hpp"

using namespace rive;

unsigned int CoreUintType::deserialize(BinaryReader& reader)
{
    return reader.readVarUintAs<unsigned int>();
}

#ifdef WITH_RIVE_TOOLS
unsigned int CoreUintType::deserializeRev(BinaryReader& reader)
{
    return deserialize(reader);
}
#endif
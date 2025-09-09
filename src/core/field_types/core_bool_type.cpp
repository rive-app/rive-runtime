#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/binary_reader.hpp"

using namespace rive;

bool CoreBoolType::deserialize(BinaryReader& reader)
{
    return reader.readByte() == 1;
}

#ifdef WITH_RIVE_TOOLS
bool CoreBoolType::deserializeRev(BinaryReader& reader)
{
    return deserialize(reader);
}
#endif
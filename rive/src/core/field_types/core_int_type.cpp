#include "core/field_types/core_int_type.hpp"
#include "core/binary_reader.hpp"

using namespace rive;

int CoreIntType::deserialize(BinaryReader& reader) 
{
    return reader.readVarInt();
}
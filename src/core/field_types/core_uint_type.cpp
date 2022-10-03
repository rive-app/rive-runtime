#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/core/binary_reader.hpp"

using namespace rive;

unsigned int CoreUintType::deserialize(BinaryReader& reader)
{
    return reader.readVarUintAs<unsigned int>();
}

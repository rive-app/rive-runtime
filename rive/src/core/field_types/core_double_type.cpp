#include "core/field_types/core_double_type.hpp"
#include "core/binary_reader.hpp"

using namespace rive;

double CoreDoubleType::deserialize(BinaryReader& reader) 
{
    return reader.lengthInBytes() == 4 ? reader.readFloat32() : reader.readFloat64();
}
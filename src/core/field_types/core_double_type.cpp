#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/binary_reader.hpp"

using namespace rive;

double CoreDoubleType::deserialize(BinaryReader& reader) 
{
    return reader.readFloat32();
}
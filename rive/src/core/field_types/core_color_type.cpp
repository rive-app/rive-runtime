#include "core/field_types/core_color_type.hpp"
#include "core/binary_reader.hpp"

using namespace rive;

int CoreColorType::deserialize(BinaryReader& reader) 
{
    return reader.readVarInt();
}
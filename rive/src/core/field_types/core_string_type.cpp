#include "core/field_types/core_string_type.hpp"
#include "core/binary_reader.hpp"

using namespace rive;

std::string CoreStringType::deserialize(BinaryReader& reader) 
{
    return reader.readString();
}
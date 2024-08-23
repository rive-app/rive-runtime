#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/binary_reader.hpp"

using namespace rive;

bool CoreBoolType::deserialize(BinaryReader& reader) { return reader.readByte() == 1; }
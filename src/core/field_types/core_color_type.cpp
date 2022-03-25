#include "rive/core/field_types/core_color_type.hpp"
#include "rive/core/binary_reader.hpp"

using namespace rive;

int CoreColorType::deserialize(BinaryReader& reader) { return reader.readUint32(); }
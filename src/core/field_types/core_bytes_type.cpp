#include "rive/core/field_types/core_bytes_type.hpp"
#include "rive/core/binary_reader.hpp"

using namespace rive;

Span<const uint8_t> CoreBytesType::deserialize(BinaryReader& reader) { return reader.readBytes(); }
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/binary_reader.hpp"

using namespace rive;

float CoreDoubleType::deserialize(BinaryReader& reader)
{
    return reader.readFloat32();
}

#ifdef WITH_RIVE_TOOLS
float CoreDoubleType::deserializeRev(BinaryReader& reader)
{
    size_t length = reader.lengthInBytes();
    if (length == 4)
    {
        return reader.readFloat32();
    }
    else if (length == 8)
    {
        return (float)reader.readFloat64();
    }
    return 0.0f;
}
#endif
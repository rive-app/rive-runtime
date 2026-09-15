#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/binary_reader.hpp"

using namespace rive;

Id CoreIdType::deserialize(BinaryReader& reader)
{
#ifdef WITH_RIVE_EDITOR
    // Editor wire format: two varuints, (client, object).
    const uint32_t client = reader.readVarUintAs<uint32_t>();
    const uint32_t object = reader.readVarUintAs<uint32_t>();
    return Id{client, object};
#else
    // Runtime-only build: single varuint index.
    return reader.readVarUintAs<Id>();
#endif
}

#ifdef WITH_RIVE_TOOLS
Id CoreIdType::deserializeRev(BinaryReader& reader)
{
    return deserialize(reader);
}
#endif

Id CoreIdType::runtimeDeserialize(BinaryReader& reader)
{
#ifdef WITH_RIVE_EDITOR
    // `.riv` wire format read into an editor build: the on-wire value
    // is the object's flat index in the file's serialized order. The
    // promoting constructor projects it to client 0 so the editor's
    // resolver routes it through the artboard's `m_Objects` array
    // (same as a runtime-only build) while coop-delivered objects use
    // non-zero clients, and maps the unset sentinel to `kEmptyId`.
    return Id(reader.readVarUintAs<uint32_t>());
#else
    return reader.readVarUintAs<Id>();
#endif
}

#ifndef _RIVE_CORE_ID_TYPE_HPP_
#define _RIVE_CORE_ID_TYPE_HPP_

#include "rive/core/id.hpp"

namespace rive
{
class BinaryReader;

// Field-type class for `Id`-typed properties (parentId, targetId, etc.).
//
// On the `.riv` wire (runtime file format) an Id is a single varuint —
// the object's index in the containing artboard's flat object list.
// On the `.rev` wire (editor coop format) an Id is two varuints —
// a `(client, object)` pair, Rive's stable per-peer identity.
//
// This class mirrors `packages/core/lib/field_types/core_id_type.dart`
// on the Dart side. The generator uses `deserialize` in editor-mode
// paths (applyChange) and `runtimeDeserialize` in runtime-load paths.
class CoreIdType
{
public:
    // TOC size class. Shares with `CoreUintType` (varuint-like). The
    // size-class enum in the file-format table of contents is `uint`
    // for both — only the wire interpretation differs.
    static const int id = 0;

    // Editor wire format: `(client, object)` as two varuints.
    // Runtime compilation aliases `Id` to `uint32_t` so this reads
    // a single varuint today; the two-varuint behavior lands in the
    // editor-mode struct flip (Step 3 of the refactor).
    static Id deserialize(BinaryReader& reader);

#ifdef WITH_RIVE_TOOLS
    // Same as `deserialize` but explicitly named for the `.rev` path —
    // matches the `deserializeRev` pattern used by other field types.
    static Id deserializeRev(BinaryReader& reader);
#endif

    // Runtime wire format (`.riv` file): single varuint.
    // In an editor build this synthesizes `Id{0, index}` so runtime-
    // loaded objects can still be resolved through the editor-mode
    // CoopId map. In a pure runtime build it's just the index.
    static Id runtimeDeserialize(BinaryReader& reader);
};
} // namespace rive

#endif

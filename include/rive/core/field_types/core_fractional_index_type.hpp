#ifndef _RIVE_CORE_FRACTIONAL_INDEX_TYPE_HPP_
#define _RIVE_CORE_FRACTIONAL_INDEX_TYPE_HPP_

#ifdef WITH_RIVE_EDITOR

#include "rive/core/binary_reader.hpp"
#include "rive/core/fractional_index.hpp"

namespace rive
{
// Field-type wrapper for `FractionalIndex`-typed properties
// (`Component.childOrder`). Editor-only — the runtime generator marks
// every FractionalIndex property as `runtime: false`, so this header
// is never reached from a pure-runtime build.
//
// Mirrors `packages/core/lib/field_types/core_fractional_index_type.dart`
// on the Dart side. Wire format is two varuints, `(numerator,
// denominator)`, written by editor_native's `handleFractionalIndexChanging`
// and read here.
class CoreFractionalIndexType
{
public:
    // TOC size class: shares with `CoreUintType` (varuint-like). The
    // file-format ToC packs only the skip-class, not the semantic
    // type, so this matches `CoreUintType::id`.
    static const int id = 0;

    static FractionalIndex deserialize(BinaryReader& reader)
    {
        const int64_t numerator =
            static_cast<int64_t>(reader.readVarUintAs<uint64_t>());
        const int64_t denominator =
            static_cast<int64_t>(reader.readVarUintAs<uint64_t>());
        return FractionalIndex{numerator, denominator};
    }

    // FractionalIndex is `runtime: false`, so `runtimeDeserialize` is
    // never reached from `.riv` import. Provided for symmetry with
    // the other field types — the generator references it
    // unconditionally in `deserialize()` switch arms.
    static FractionalIndex runtimeDeserialize(BinaryReader& reader)
    {
        return deserialize(reader);
    }
};
} // namespace rive

#endif
#endif

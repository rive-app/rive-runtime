#ifndef _RIVE_CORE_DOUBLE_TYPE_HPP_
#define _RIVE_CORE_DOUBLE_TYPE_HPP_

namespace rive
{
class BinaryReader;
class CoreDoubleType
{
public:
    static const int id = 2;
    static float deserialize(BinaryReader& reader);
#if defined(WITH_RIVE_TOOLS) || defined(WITH_RIVE_EDITOR)
    // Variable-precision reader for `.rev` / coop wire — Dart's
    // `CoreDoubleType.serialize` emits float32 when the value round-
    // trips exactly, otherwise float64. Picks by buffer length so the
    // editor reads either. Used by generator-emitted `applyChange`
    // (see
    // `runtime/dev/core_generator/lib/src/field_types/double_field_type.dart`).
    static float deserializeRev(BinaryReader& reader);
#endif
};
} // namespace rive
#endif
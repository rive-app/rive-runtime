import '../field_type.dart';

/// Field type for object references (`parentId`, `targetId`, `styleId`,
/// etc.). Mirrors `packages/core/lib/field_types/core_id_type.dart` on
/// the Dart side.
///
/// The emitted C++ storage type is `rive::Id`, which `rive/core/id.hpp`
/// defines as `uint32_t` in runtime-only builds and a `{client, object}`
/// struct under `WITH_RIVE_EDITOR`. The generator therefore doesn't
/// need to branch on mode — both builds see the same `Id` identifier,
/// and the struct has an implicit constructor from `uint32_t` so
/// backward-compatible call sites keep working.
///
/// The wire format DOES differ per mode:
///   - runtime `.riv`: single varuint (flat index)
///   - editor `.rev`: two varuints (client, object)
///
/// `runtimeDeserializeFunction` picks the first read path (used in
/// `deserialize(propertyKey, reader)`), `deserializeFunction` picks
/// the second (used in `applyChange(propertyKey, reader)` under
/// `WITH_RIVE_EDITOR`). For every other field type the two are
/// identical — only `Id` needs the split.
class IdFieldType extends FieldType {
  IdFieldType()
      : super(
          'Id',
          'CoreIdType',
          cppName: 'Id',
          include: 'rive/core/id.hpp',
        );

  @override
  String get defaultValue => 'kEmptyId';

  @override
  String get deserializeFunction => '$runtimeCoreType::deserialize';

  @override
  String get runtimeDeserializeFunction =>
      '$runtimeCoreType::runtimeDeserialize';

  // `initialValue`/`initialValueRuntime` in the JSON defs are shaped
  // for the runtime-mode `uint` typeRuntime and vary per field:
  //   - `"-1"` (== UINT32_MAX) signals "unset" (listener inputId,
  //     targetId fields, etc.) — rewrite to `kEmptyId`.
  //   - `"0"` signals a legitimate index-0 reference in many
  //     component defs (Component.parentId defaults to the artboard
  //     at m_Objects[0]); leave alone so `resolve(Id{0, 0})` keeps
  //     returning that first object.
  //   - `"Core.missingId"` is the Dart editor's empty-sentinel name —
  //     always rewrite to the C++ equivalent.
  // Rewriting the unset sentinels in place keeps the
  // `captureStateForJournal`-style "is this at default?" check
  // unambiguous without requiring a sweep of the JSON defs.
  @override
  String? convertCpp(String value) {
    if (value == '-1' || value == 'Core.missingId') {
      return 'kEmptyId';
    }
    return value;
  }
}

import '../field_type.dart';

/// Field type for `FractionalIndex`-typed properties (currently
/// `Component.childOrder`). Mirrors
/// `packages/core/lib/field_types/core_fractional_index_type.dart` on
/// the Dart side.
///
/// The emitted C++ storage type is `rive::FractionalIndex` — defined
/// in `rive/core/fractional_index.hpp` under `#ifdef WITH_RIVE_EDITOR`.
/// Every property using this type is `runtime: false` in the JSON
/// defs, so the generator only emits storage / accessors / wire reads
/// under that same guard via `propEditorGuard` / `startPropertyGuards`.
///
/// Wire format: two varuints `(numerator, denominator)`. There's no
/// runtime path — `.riv` files never carry a FractionalIndex — but
/// `runtimeDeserializeFunction` falls back to `deserialize` for
/// symmetry with the other field types.
class FractionalIndexFieldType extends FieldType {
  FractionalIndexFieldType()
      : super(
          'FractionalIndex',
          'CoreFractionalIndexType',
          cppName: 'FractionalIndex',
          include: 'rive/core/fractional_index.hpp',
        );

  // Both `FractionalIndex` and `CoreFractionalIndexType` are defined
  // under `#ifdef WITH_RIVE_EDITOR`, so the registry's whole
  // set/get/field-type-id surface for this type must be guarded — not
  // just the per-property cases.
  @override
  bool get isWithRiveEditorOnly => true;

  @override
  String get defaultValue => 'FractionalIndex::invalid()';

  // JSON defs may carry the Dart sentinel `FractionalIndex.invalid` or
  // legacy `initialValueRuntime: "0"` (a pre-refactor convention for
  // "unset" — the property's `runtime: false` flag means it never
  // appears in `.riv` files anyway). Either way, rewrite to the C++
  // sentinel `FractionalIndex::invalid()` so the storage initializer
  // and `captureStateForJournal`'s non-default check compile + agree.
  @override
  String? convertCpp(String value) {
    if (value == 'FractionalIndex.invalid' ||
        value == '0' ||
        value == '-1') {
      return 'FractionalIndex::invalid()';
    }
    return value;
  }
}

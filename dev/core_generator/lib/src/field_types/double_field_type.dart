import '../field_type.dart';

class DoubleFieldType extends FieldType {
  DoubleFieldType() : super('double', 'CoreDoubleType', cppName: 'float');

  @override
  String get defaultValue => '0.0f';

  /// Coop wire variant: Dart's `CoreDoubleType.serialize` picks float32
  /// or float64 per value (see `rive_core`'s own fieldType). The runtime
  /// wire (`deserialize`) is always float32; the editor `applyChange`
  /// path must dispatch on buffer length, so point it at
  /// `deserializeRev` (declared under `WITH_RIVE_EDITOR` in
  /// `packages/runtime/include/rive/core/field_types/core_double_type.hpp`).
  @override
  String get deserializeFunction => 'CoreDoubleType::deserializeRev';

  @override
  String? convertCpp(String value) {
    var result = value;
    if (result.isNotEmpty) {
      if (result[result.length - 1] != 'f') {
        if (!result.contains('.')) {
          result += '.0';
        }
        result += 'f';
      }
    }
    return result;
  }
}

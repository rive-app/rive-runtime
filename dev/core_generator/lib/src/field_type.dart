export 'package:core_generator/src/field_types/bool_field_type.dart';
export 'package:core_generator/src/field_types/color_field_type.dart';
export 'package:core_generator/src/field_types/double_field_type.dart';
export 'package:core_generator/src/field_types/fractional_index_field_type.dart';
export 'package:core_generator/src/field_types/id_field_type.dart';
export 'package:core_generator/src/field_types/string_field_type.dart';
export 'package:core_generator/src/field_types/uint_field_type.dart';
export 'package:core_generator/src/field_types/uint64_field_type.dart';

Map<String, FieldType> _types = <String, FieldType>{};

abstract class FieldType {
  final String name;
  String? _cppName;
  final String? include;
  String? get cppName => _cppName;
  String? get cppGetterName => _cppName;

  final String _runtimeCoreType;
  String get runtimeCoreType => _runtimeCoreType;

  /// Fully-qualified C++ function to read this field type from the
  /// `.rev` (editor / coop) wire. Emitted inside
  /// `applyChange(propertyKey, reader)` under `WITH_RIVE_EDITOR`. For
  /// almost every field type this matches the runtime wire read (see
  /// `runtimeDeserializeFunction`); `Id`-typed fields override both
  /// so the editor wire can read two varuints (client, object) while
  /// the runtime wire reads a single varuint.
  String get deserializeFunction => '$_runtimeCoreType::deserialize';

  /// Fully-qualified C++ function to read this field type from the
  /// `.riv` (runtime) wire. Emitted inside
  /// `deserialize(propertyKey, reader)`.
  String get runtimeDeserializeFunction => '$_runtimeCoreType::deserialize';
  /// The field type that owns registry dispatch and the property field-type id
  /// for this type. Defaults to the type itself; narrow aliases (e.g. uint8)
  /// override this to fold into a wider type (uint) so dispatch and the wire
  /// format stay shared.
  FieldType get registryType => this;

  final bool storesData;

  /// Byte width of the generated C++ storage, for the fixed-width integer
  /// types only. Null everywhere else — the editor's property hook passes
  /// a pointer to the field, so only these need a width to read it back.
  int? get storageBytes => const <String, int>{
        'int8_t': 1,
        'uint8_t': 1,
        'int16_t': 2,
        'uint16_t': 2,
        'int32_t': 4,
        'uint32_t': 4,
        'uint64_t': 8,
      }[cppName];

  /// True when the C++ storage type (`cppName`) and core-type
  /// (`runtimeCoreType`) only exist in `WITH_RIVE_EDITOR` builds. The
  /// registry's `set${Name}` / `get${Name}` methods and the
  /// field-type-id switch case naming this type must then be guarded
  /// wholesale — not just their per-property cases — or the
  /// runtime-only build fails to find the undefined type. See
  /// `FractionalIndexFieldType`.
  bool get isWithRiveEditorOnly => false;

  FieldType(
    this.name,
    this._runtimeCoreType, {
    String? cppName,
    this.include,
    this.storesData = true,
  }) {
    _cppName = cppName ?? name;
    _types[name] = this;
  }

  static FieldType? find(dynamic key) {
    if (key is! String) {
      return null;
    }
    return _types[key];
  }

  @override
  String toString() {
    return name;
  }

  String equalityCheck(String varAName, String varBName) {
    return '$varAName == $varBName';
  }

  String? get defaultValue => null;

  String get uncapitalizedName => '${name[0].toLowerCase()}${name.substring(1)}'
      .replaceAll('<', '')
      .replaceAll('>', '');

  String get capitalizedName => '${name[0].toUpperCase()}${name.substring(1)}'
      .replaceAll('<', '')
      .replaceAll('>', '');

  String get snakeName => name
      .replaceAllMapped(RegExp('(.+?)([A-Z])'), (Match m) => '${m[1]}_${m[2]}')
      .toLowerCase();

  String get snakeRuntimeCoreName => _runtimeCoreType
      .replaceAllMapped(RegExp('(.+?)([A-Z])'), (Match m) => '${m[1]}_${m[2]}')
      .toLowerCase();

  String? convertCpp(String value) => value;
}

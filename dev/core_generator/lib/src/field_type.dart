export 'package:core_generator/src/field_types/bool_field_type.dart';
export 'package:core_generator/src/field_types/color_field_type.dart';
export 'package:core_generator/src/field_types/double_field_type.dart';
export 'package:core_generator/src/field_types/string_field_type.dart';
export 'package:core_generator/src/field_types/uint_field_type.dart';

Map<String, FieldType> _types = <String, FieldType>{};

abstract class FieldType {
  final String name;
  String? _cppName;
  final String? include;
  String? get cppName => _cppName;
  String? get cppGetterName => _cppName;

  final String _runtimeCoreType;
  String get runtimeCoreType => _runtimeCoreType;

  final bool storesData;

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

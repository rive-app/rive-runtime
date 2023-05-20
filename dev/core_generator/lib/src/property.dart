import 'package:colorize/colorize.dart';
import 'package:core_generator/src/definition.dart';
import 'package:core_generator/src/field_type.dart';
import 'package:core_generator/src/key.dart';

class Property {
  final String name;
  final FieldType type;
  final Definition definition;
  String? initialValue;
  String? initialValueRuntime;
  bool isVirtual = false;
  bool animates = false;
  String? group;
  Key? key;
  String? description;
  bool isNullable = false;
  bool isRuntime = true;
  bool isCoop = true;
  bool isSetOverride = false;
  bool isGetOverride = false;
  bool isEncoded = false;
  FieldType? typeRuntime;

  static Property? make(
      Definition type, String name, Map<String, dynamic> data) {
    if (data['runtime'] is bool && data['runtime'] == false) {
      return null;
    }

    var fieldType =
        FieldType.find(data['typeRuntime']) ?? FieldType.find(data['type']);

    if (fieldType == null) {
      color('Invalid field type ${data['type']} for $name.', front: Styles.RED);
      return null;
    }
    return Property(type, name, fieldType, data);
  }

  Property(this.definition, this.name, this.type, Map<String, dynamic> data) {
    dynamic encodedValue = data['encoded'];
    if (encodedValue is bool) {
      isEncoded = encodedValue;
    }
    dynamic descriptionValue = data['description'];
    if (descriptionValue is String) {
      description = descriptionValue;
    }
    dynamic nullableValue = data['nullable'];
    if (nullableValue is bool) {
      isNullable = nullableValue;
    }
    dynamic init = data['initialValue'];
    if (init is String) {
      initialValue = init;
    }
    dynamic initRuntime = data['initialValueRuntime'];
    if (initRuntime is String) {
      initialValueRuntime = initRuntime;
    }
    dynamic overrideSet = data['overrideSet'];
    if (overrideSet is bool && overrideSet) {
      isSetOverride = true;
    }
    dynamic overrideGet = data['overrideGet'];
    if (overrideGet is bool && overrideGet) {
      isGetOverride = true;
    }
    dynamic a = data['animates'];
    if (a is bool) {
      animates = a;
    }
    dynamic virtualValue = data['virtual'];
    isVirtual = virtualValue is bool && virtualValue;
    dynamic g = data['group'];
    if (g is String) {
      group = g;
    }
    dynamic e = data['editorOnly'];
    if (e is bool && e) {
      isCoop = false;
    }
    dynamic r = data['runtime'];
    if (r is bool) {
      isRuntime = r;
    }
    dynamic c = data['coop'];
    if (c is bool) {
      isCoop = c;
    }
    dynamic rt = data['typeRuntime'];
    if (rt is String) {
      typeRuntime = FieldType.find(rt);
    }
    key = Key.fromJSON(data['key']) ?? Key.forProperty(this);
  }

  FieldType getExportType() => typeRuntime ?? type;

  @override
  String toString() => '$name(${key?.intValue})';

  String get capitalizedName => '${name[0].toUpperCase()}${name.substring(1)}'
      .replaceAll('<', '')
      .replaceAll('>', '');
}

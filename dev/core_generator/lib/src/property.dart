import 'package:colorize/colorize.dart';
import 'package:core_generator/src/comment.dart';
import 'package:core_generator/src/definition.dart';
import 'package:core_generator/src/field_type.dart';
import 'package:core_generator/src/key.dart';

class Property {
  final String name;
  final FieldType type;
  final Definition definition;
  String initialValue;
  String initialValueRuntime;
  bool isVirtual = false;
  bool animates = false;
  String group;
  Key key;
  String description;
  bool isNullable = false;
  bool isRuntime = true;
  bool isCoop = true;
  bool isSetOverride = false;
  bool isGetOverride = false;
  FieldType typeRuntime;

  factory Property(Definition type, String name, Map<String, dynamic> data) {
    if (data['runtime'] is bool && data['runtime'] == false) {
      return null;
    }

    var fieldType =
        FieldType.find(data["typeRuntime"]) ?? FieldType.find(data["type"]);

    if (fieldType == null) {
      color('Invalid field type ${data['type']} for $name.', front: Styles.RED);
      return null;
    }
    return Property.make(type, name, fieldType, data);
  }

  Property.make(
      this.definition, this.name, this.type, Map<String, dynamic> data) {
    dynamic descriptionValue = data["description"];
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
    key = Key.fromJSON(data["key"]) ?? Key.forProperty(this);
  }

  FieldType getExportType() => typeRuntime ?? type;

  String generateCode(bool forRuntime) {
    bool exportAnimates = false;
    var exportType = getExportType();
    String propertyKey = '${name}PropertyKey';
    var code = StringBuffer('  /// ${'-' * 74}\n');
    code.write(comment('${capitalize(name)} field with key ${key.intValue}.',
        indent: 1));
    if (initialValueRuntime != null || initialValue != null) {
      code.writeln(
          '${exportType.cppName} _$name = ${initialValueRuntime ?? initialValue};');
    } else {
      code.writeln('${exportType.cppName} _$name;');
    }
    if (exportAnimates) {
      code.writeln('${exportType.cppName} _${name}Animated;');
      code.writeln('KeyState _${name}KeyState = KeyState.none;');
    }
    code.writeln('static const int $propertyKey = ${key.intValue};');

    if (description != null) {
      code.write(comment(description, indent: 1));
    }
    if (exportAnimates) {
      code.write(comment(
          'Get the [_$name] field value.'
          'Note this may not match the core value '
          'if animation mode is active.',
          indent: 1));
      code.writeln(
          '${exportType.cppName} get $name => _${name}Animated ?? _$name;');
      code.write(
          comment('Get the non-animation [_$name] field value.', indent: 1));
      code.writeln('${exportType.cppName} get ${name}Core => _$name;');
    } else {
      code.writeln('${exportType.cppName} get $name => _$name;');
    }
    code.write(comment('Change the [_$name] field value.', indent: 1));
    code.write(comment(
        '[${name}Changed] will be invoked only if the '
        'field\'\s value has changed.',
        indent: 1));
    code.writeln(
        '''set $name${exportAnimates ? 'Core' : ''}(${exportType.cppName} value) {
        if(${exportType.equalityCheck('_$name', 'value')}) { return; }
        ${exportType.cppName} from = _$name;
        _$name = value;''');
    // Property change callbacks to the context don't propagate at runtime.
    if (!forRuntime) {
      code.writeln('onPropertyChanged($propertyKey, from, value);');
      if (!isCoop) {
        code.writeln(
            'context?.editorPropertyChanged(this, $propertyKey, from, value);');
      }
    }
    // Change callbacks do as we use those to trigger dirty states.
    code.writeln('''
        ${name}Changed(from, value);
      }''');
    if (exportAnimates) {
      code.writeln('''set $name(${exportType.cppName} value) {
        if(context != null && context.isAnimating && $name != value) {
          _${name}Animate(value, true);
          return;
        }
        ${name}Core = value;
      }''');

      code.writeln(
          '''void _${name}Animate(${exportType.cppName} value, bool autoKey) {
        if (_${name}Animated == value) {
          return;
        }
        ${exportType.cppName} from = $name;
        _${name}Animated = value;
        ${exportType.cppName} to = $name;
        onAnimatedPropertyChanged($propertyKey, autoKey, from, to);
        ${name}Changed(from, to);
      }''');

      code.writeln(
          '${exportType.cppName} get ${name}Animated => _${name}Animated;');
      code.writeln('''set ${name}Animated(${exportType.cppName} value) =>
                        _${name}Animate(value, false);''');
      code.writeln('KeyState get ${name}KeyState => _${name}KeyState;');
      code.writeln('''set ${name}KeyState(KeyState value) {
        if (_${name}KeyState == value) {
          return;
        }
        _${name}KeyState = value;
        // Force update anything listening on this property.
        onAnimatedPropertyChanged($propertyKey, false, _${name}Animated, _${name}Animated);
      }''');
    }
    code.writeln('void ${name}Changed('
        '${exportType.cppName} from, ${exportType.cppName} to);\n');

    return code.toString();
  }

  Map<String, dynamic> serialize() {
    Map<String, dynamic> data = <String, dynamic>{'type': type.name};
    if (typeRuntime != null) {
      data['typeRuntime'] = typeRuntime.name;
    }

    if (initialValue != null) {
      data['initialValue'] = initialValue;
    }
    if (initialValueRuntime != null) {
      data['initialValueRuntime'] = initialValueRuntime;
    }
    if (isGetOverride) {
      data['overrideGet'] = true;
    }
    if (isSetOverride) {
      data['overrideSet'] = true;
    }
    if (animates) {
      data['animates'] = true;
    }
    if (group != null) {
      data['group'] = group;
    }
    data['key'] = key.serialize();
    if (description != null) {
      data['description'] = description;
    }
    if (isNullable) {
      data['nullable'] = true;
    }
    if (!isRuntime) {
      data['runtime'] = false;
    }
    if (!isCoop) {
      data['coop'] = false;
    }
    if (isVirtual) {
      data['virtual'] = true;
    }
    return data;
  }

  @override
  String toString() {
    return '$name(${key.intValue})';
  }

  String get capitalizedName => '${name[0].toUpperCase()}${name.substring(1)}'
      .replaceAll('<', '')
      .replaceAll('>', '');
}

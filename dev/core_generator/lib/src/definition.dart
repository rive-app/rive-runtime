import 'dart:convert';
import 'dart:io';

import 'package:colorize/colorize.dart';
// import 'package:core_generator/src/field_type.dart';
// import 'package:core_generator/src/comment.dart';
import 'package:core_generator/src/key.dart';
import 'package:core_generator/src/cpp_formatter.dart';
import 'package:core_generator/src/property.dart';
import 'package:core_generator/src/configuration.dart';

String stripExtension(String filename) {
  var index = filename.lastIndexOf('.');
  return index == -1 ? filename : filename.substring(0, index);
}

class Definition {
  static final Map<String, Definition> definitions = <String, Definition>{};
  final String _filename;
  static final _formatter = CppFormatter();

  String _name;
  final List<Property> _properties = [];

  List<Property> get properties => _properties
      .where((property) => property.isRuntime)
      .toList(growable: false);

  Definition _extensionOf;
  Key _key;
  bool _isAbstract = false;
  bool _exportsWithContext = false;
  bool _editorOnly = false;
  factory Definition(String filename) {
    var definition = definitions[filename];
    if (definition != null) {
      return definition;
    }

    var file = File(defsPath + filename);
    var contents = file.readAsStringSync();
    Map<String, dynamic> definitionData;
    try {
      dynamic parsedJson = json.decode(contents);
      if (parsedJson is Map<String, dynamic>) {
        definitionData = parsedJson;
      }
    } on FormatException catch (error) {
      color('Invalid json data in $filename: $error', front: Styles.RED);
      return null;
    }
    definitions[filename] =
        definition = Definition.fromFilename(filename, definitionData);
    return definition;
  }
  Definition.fromFilename(this._filename, Map<String, dynamic> data) {
    dynamic extendsFilename = data['extends'];
    if (extendsFilename is String) {
      _extensionOf = Definition(extendsFilename);
    }
    dynamic nameValue = data['name'];
    if (nameValue is String) {
      _name = nameValue;
    }
    dynamic abstractValue = data['abstract'];
    if (abstractValue is bool) {
      _isAbstract = abstractValue;
    }
    dynamic editorOnlyValue = data['editorOnly'];
    if (editorOnlyValue is bool) {
      _editorOnly = editorOnlyValue;
    }
    dynamic exportsWithContextValue = data['exportsWithContext'];
    if (exportsWithContextValue is bool) {
      _exportsWithContext = exportsWithContextValue;
    }
    _key = Key.fromJSON(data['key']) ?? Key.forDefinition(this);

    dynamic properties = data['properties'];
    if (properties is Map<String, dynamic>) {
      for (final MapEntry<String, dynamic> entry in properties.entries) {
        if (entry.value is Map<String, dynamic>) {
          var property =
              Property(this, entry.key, entry.value as Map<String, dynamic>);
          if (property == null) {
            continue;
          }
          _properties.add(property);
        }
      }
    }
  }
  String get localFilename => _filename.indexOf(defsPath) == 0
      ? _filename.substring(defsPath.length)
      : _filename;

  String get name => _name;

  String get localCodeFilename => '${stripExtension(_filename)}_base.hpp';
  String get concreteCodeFilename => '${stripExtension(_filename)}.hpp';
  String get codeFilename => 'lib/src/generated/$localCodeFilename';

  /// Generates cpp header code based on the Definition
  Future<void> generateCode() async {
    bool defineContextExtension = _extensionOf?._name == null;
    StringBuffer code = StringBuffer();
    if (defineContextExtension) {
      code.writeln('#include "core.hpp"');
    } else {
      code.writeln('#include "${_extensionOf.concreteCodeFilename}"');
    }
    code.writeln('namespace rive {');
    code.writeln('class ${_name}Base : public '
        '${defineContextExtension ? 'Core' : _extensionOf?._name} {};');
    code.writeln('}');

    var file = File('$generatedHppPath$localCodeFilename');
    file.createSync(recursive: true);

    var formattedCode =
        await _formatter.formatAndGuard('${_name}Base', code.toString());
    file.writeAsStringSync(formattedCode, flush: true);

    // See if we need to stub out the concrete version...
    var concreteFile = File('$concreteHppPath$concreteCodeFilename');
    if (true) {
      //!concreteFile.existsSync()) {
      StringBuffer concreteCode = StringBuffer();
      concreteFile.createSync(recursive: true);
      concreteCode.writeln('#include "generated/$localCodeFilename"');
      concreteCode.writeln('#include <stdio.h>');
      concreteCode.writeln('namespace rive {');
      concreteCode.writeln('''class $_name : public ${_name}Base {
        public:
          $_name() {
            printf("Constructing $_name\\n");
          }
      };''');
      concreteCode.writeln('}');

      var formattedCode =
          await _formatter.formatAndGuard(_name, concreteCode.toString());
      concreteFile.writeAsStringSync(formattedCode, flush: true);
    }
  }

  @override
  String toString() {
    return '$_name[${_key?.intValue ?? '-'}]';
  }

  static const int minPropertyId = 3;
  static Future<bool> generate() async {
    // Check dupe ids.
    bool runGenerator = true;
    Map<int, Definition> ids = {};
    Map<int, Property> properties = {};
    for (final definition in definitions.values) {
      if (definition._key?.intValue != null) {
        var other = ids[definition._key.intValue];
        if (other != null) {
          color('Duplicate type ids for $definition and $other.',
              front: Styles.RED);
          runGenerator = false;
        } else {
          ids[definition._key.intValue] = definition;
        }
      }
      for (final property in definition._properties) {
        if (property.key.isMissing) {
          continue;
        }
        var other = properties[property.key.intValue];
        if (other != null) {
          color(
              '''Duplicate field ids for ${property.definition}.$property '''
              '''and ${other.definition}.$other.''',
              front: Styles.RED);
          runGenerator = false;
        } else if (property.key.intValue < minPropertyId) {
          color(
              '${property.definition}.$property: ids less than '
              '$minPropertyId are reserved.',
              front: Styles.RED);
          runGenerator = false;
        } else {
          properties[property.key.intValue] = property;
        }
      }
    }

    // Find max id, we use this to assign to types that don't have ids yet.
    int nextFieldId = minPropertyId - 1;
    int nextId = 0;
    for (final definition in definitions.values) {
      if (definition._key != null &&
          definition._key.intValue != null &&
          definition._key.intValue > nextId) {
        nextId = definition._key.intValue;
      }
      for (final field in definition._properties) {
        if (field != null &&
            field.key.intValue != null &&
            field.key.intValue > nextFieldId) {
          nextFieldId = field.key.intValue;
        }
      }
    }

    if (!runGenerator) {
      color('Not running generator due to previous errors.',
          front: Styles.YELLOW);
      return false;
    }

    definitions.removeWhere((key, definition) => definition._editorOnly);

    // Clear out previous generated code.
    var dir = Directory(generatedHppPath);
    if (dir.existsSync()) {
      dir.deleteSync(recursive: true);
    }
    dir.createSync(recursive: true);
    // Generate core context.

    for (final definition in definitions.values) {
      await definition.generateCode();
    }

    StringBuffer ctxCode =
        StringBuffer('namespace rive {class CoreContext {};}');

    var output = generatedHppPath;
    var folder =
        output != null && output.isNotEmpty && output[output.length - 1] == '/'
            ? output.substring(0, output.length - 1)
            : output;

    var file = File('$folder/core_context.hpp');
    file.createSync(recursive: true);

    var formattedCode =
        await _formatter.formatAndGuard('CoreContext', ctxCode.toString());
    file.writeAsStringSync(formattedCode, flush: true);

    return true;
  }
}

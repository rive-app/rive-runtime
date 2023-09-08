import 'dart:convert';
import 'dart:io';

import 'package:colorize/colorize.dart';
import 'package:core_generator/src/comment.dart';
import 'package:core_generator/src/configuration.dart';
import 'package:core_generator/src/cpp_formatter.dart';
import 'package:core_generator/src/field_type.dart';
import 'package:core_generator/src/key.dart';
import 'package:core_generator/src/property.dart';

String stripExtension(String filename) {
  var index = filename.lastIndexOf('.');
  return index == -1 ? filename : filename.substring(0, index);
}

class Definition {
  static final Map<String, Definition> definitions = <String, Definition>{};
  final String _filename;
  static final _formatter = CppFormatter();

  String? _name;
  final List<Property> _properties = [];

  List<Property> get properties => _properties
      .where((property) => property.isRuntime)
      .toList(growable: false);

  Iterable<Property> get storedProperties =>
      properties.where((property) => property.getExportType().storesData);

  Definition? _extensionOf;
  Key? _key;
  bool _isAbstract = false;
  bool _editorOnly = false;
  bool _forRuntime = true;
  bool get forRuntime => _forRuntime;
  static Definition? make(String filename) {
    var definition = definitions[filename];
    if (definition != null) {
      return definition;
    }

    var file = File(defsPath + filename);
    var contents = file.readAsStringSync();
    late Map<String, dynamic> definitionData;
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
      _extensionOf = Definition.make(extendsFilename);
    }
    dynamic nameValue = data['name'];
    if (nameValue is String) {
      _name = nameValue;
    }
    dynamic forRuntime = data['runtime'];
    if (forRuntime is bool) {
      _forRuntime = forRuntime;
    }
    dynamic abstractValue = data['abstract'];
    if (abstractValue is bool) {
      _isAbstract = abstractValue;
    }
    dynamic editorOnlyValue = data['editorOnly'];
    if (editorOnlyValue is bool) {
      _editorOnly = editorOnlyValue;
    }
    _key = Key.fromJSON(data['key']) ?? Key.forDefinition(this);

    dynamic properties = data['properties'];
    if (properties is Map<String, dynamic>) {
      for (final MapEntry<String, dynamic> entry in properties.entries) {
        if (entry.value is Map<String, dynamic>) {
          var property = Property.make(
              this, entry.key, entry.value as Map<String, dynamic>);
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

  String? get name => _name;

  String get localCodeFilename => '${stripExtension(_filename)}_base.hpp';
  String get concreteCodeFilename => 'rive/${stripExtension(_filename)}.hpp';
  String get localCppCodeFilename => '${stripExtension(_filename)}_base.cpp';

  /// Generates cpp header code based on the Definition
  Future<void> generateCode() async {
    if (!_forRuntime) {
      return;
    }
    bool defineContextExtension = _extensionOf?._name == null;
    StringBuffer code = StringBuffer();

    var includes = <String>{
      defineContextExtension
          ? 'rive/core.hpp'
          : _extensionOf!.concreteCodeFilename
    };
    for (final property in properties) {
      var include = property.type.include;
      if (include != null) {
        includes.add(include);
      }
      includes.add('rive/core/field_types/' +
          property.type.snakeRuntimeCoreName +
          '.hpp');
    }

    var sortedIncludes = includes.toList()..sort();
    for (final include in sortedIncludes) {
      code.write('#include ');
      if (include[0] == '<') {
        code.write(include);
      } else {
        code.write('\"$include\"');
      }
      code.write('\n');
    }

    code.writeln('namespace rive {');
    var superTypeName = defineContextExtension ? 'Core' : _extensionOf?._name;
    code.writeln('class ${_name}Base : public $superTypeName {');

    code.writeln('protected:');
    code.writeln('typedef $superTypeName Super;');
    code.writeln('public:');
    code.writeln('static const uint16_t typeKey = ${_key!.intValue};\n');

    code.write(comment(
        'Helper to quickly determine if a core object extends another '
        'without RTTI at runtime.',
        indent: 1));
    code.writeln('bool isTypeOf(uint16_t typeKey) const override {');

    code.writeln('switch(typeKey) {');
    code.writeln('case ${_name}Base::typeKey:');
    for (var p = _extensionOf; p != null; p = p._extensionOf) {
      code.writeln('case ${p._name}Base::typeKey:');
    }
    code.writeln('return true;');
    code.writeln('default: return false;}');

    code.writeln('}\n');

    code.writeln('uint16_t coreType() const override { return typeKey; }\n');
    if (properties.isNotEmpty) {
      for (final property in properties) {
        code.writeln('static const uint16_t ${property.name}PropertyKey = '
            '${property.key!.intValue};');
      }
      if (storedProperties.any((prop) => !prop.isEncoded)) {
        code.writeln('private:');
      }

      // Write fields.
      for (final property in properties) {
        if (property.isEncoded || !property.getExportType().storesData) {
          // Encoded properties don't store data, it's up to the implementation
          // to decode and store what it needs.
          continue;
        }
        code.writeln('${property.type.cppName} m_${property.capitalizedName}');

        var initialize = property.initialValueRuntime ??
            property.initialValue ??
            property.type.defaultValue;
        if (initialize != null) {
          var converted = property.type.convertCpp(initialize);
          if (converted != null) {
            code.write(' = $converted');
          }
        }
        code.write(';');
      }

      // Write getter/setters.
      code.writeln('public:');
      for (final property in properties) {
        if (!property.getExportType().storesData) {
          code.writeln((property.isSetOverride ? '' : 'virtual ') +
              'void ${property.name}' +
              '(const ${property.type.cppName}& value) ' +
              (property.isSetOverride ? 'override' : '') +
              '= 0;');
        } else if (property.isEncoded) {
          // Encoded properties just have a pure virtual decoder that needs to
          // be implemented. Also requires an implemention of copyPropertyName
          // as that will no longer automatically be copied by the generated
          // code.
          code.writeln((property.isSetOverride ? '' : 'virtual ') +
              'void decode${property.capitalizedName}' +
              '(${property.type.cppName} value) ' +
              (property.isSetOverride ? 'override' : '') +
              '= 0;');
          code.writeln((property.isSetOverride ? '' : 'virtual ') +
              'void copy${property.capitalizedName}' +
              '(const ${_name}Base& object) ' +
              (property.isSetOverride ? 'override' : '') +
              '= 0;');
        } else {
          code.writeln((property.isVirtual ? 'virtual' : 'inline') +
              ' ${property.type.cppGetterName} ${property.name}() const ' +
              (property.isGetOverride ? 'override' : '') +
              '{ return m_${property.capitalizedName}; }');

          code.writeln(
              'void ${property.name}(${property.type.cppName} value) ' +
                  (property.isSetOverride ? 'override' : '') +
                  '{'
                      'if(m_${property.capitalizedName} == value)'
                      '{return;}'
                      'm_${property.capitalizedName} = value;'
                      '${property.name}Changed();'
                      '}');
        }

        code.writeln();
      }
    }

    if (!_isAbstract) {
      code.writeln('Core* clone() const override;');
    }

    if (storedProperties.isNotEmpty || _extensionOf == null) {
      code.writeln('void copy(const ${_name}Base& object) {');
      for (final property in storedProperties) {
        if (property.isEncoded) {
          code.writeln('copy${property.capitalizedName}(object);');
        } else {
          code.writeln('m_${property.capitalizedName} = '
              'object.m_${property.capitalizedName};');
        }
      }
      if (_extensionOf != null) {
        code.writeln('${_extensionOf!.name}::'
            'copy(object); ');
      }
      code.writeln('}');
      code.writeln();

      code.writeln('bool deserialize(uint16_t propertyKey, '
          'BinaryReader& reader) override {');

      if (storedProperties.isNotEmpty) {
        code.writeln('switch (propertyKey){');
        for (final property in properties) {
          code.writeln('case ${property.name}PropertyKey:');
          if (property.isEncoded) {
            code.writeln('decode${property.capitalizedName}'
                '(${property.type.runtimeCoreType}::deserialize(reader));');
          } else {
            code.writeln('m_${property.capitalizedName} = '
                '${property.type.runtimeCoreType}::deserialize(reader);');
          }
          code.writeln('return true;');
        }
        code.writeln('}');
      }
      if (_extensionOf != null) {
        code.writeln('return ${_extensionOf!.name}::'
            'deserialize(propertyKey, reader); }');
      } else {
        code.writeln('return false; }');
      }
    }

    code.writeln('protected:');
    if (storedProperties.isNotEmpty) {
      for (final property in storedProperties) {
        code.writeln('virtual void ${property.name}Changed() {}');
      }
    }
    code.writeln('};');
    code.writeln('}');

    var file = File('$generatedHppPath$localCodeFilename');
    file.createSync(recursive: true);

    var formattedCode =
        await _formatter.formatAndGuard('${_name}Base', code.toString());
    file.writeAsStringSync(formattedCode, flush: true);

    // See if we need to stub out the concrete version...
    var concreteFile = File('$concreteHppPath$concreteCodeFilename');
    if (!concreteFile.existsSync()) {
      StringBuffer concreteCode = StringBuffer();
      concreteFile.createSync(recursive: true);
      concreteCode.writeln('#include "rive/generated/$localCodeFilename"');
      concreteCode.writeln('#include <stdio.h>');
      concreteCode.writeln('namespace rive {');
      concreteCode.writeln('''class $_name : public ${_name}Base {
        public:
      };''');
      concreteCode.writeln('}');

      var formattedCode =
          await _formatter.formatAndGuard(_name!, concreteCode.toString());
      concreteFile.writeAsStringSync(formattedCode, flush: true);
    }
    if (!_isAbstract) {
      StringBuffer cppCode = StringBuffer();
      cppCode.writeln('#include "rive/generated/$localCodeFilename"');
      cppCode.writeln('#include "$concreteCodeFilename"');
      cppCode.writeln();
      cppCode.writeln('using namespace rive;');
      cppCode.writeln();
      cppCode.writeln('Core* ${_name}Base::clone() const { '
          'auto cloned = new $_name(); '
          'cloned->copy(*this); '
          'return cloned; '
          '}');
      var cppFile = File('$generatedCppPath$localCppCodeFilename');
      cppFile.createSync(recursive: true);
      var formattedCode = await _formatter.format(cppCode.toString());
      cppFile.writeAsStringSync(formattedCode, flush: true);
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
        var other = ids[definition._key!.intValue];
        if (other != null) {
          color('Duplicate type ids for $definition and $other.',
              front: Styles.RED);
          runGenerator = false;
        } else {
          ids[definition._key!.intValue!] = definition;
        }
      }
      for (final property in definition._properties) {
        if (property.key!.isMissing) {
          continue;
        }
        var other = properties[property.key!.intValue];
        if (other != null) {
          color(
              '''Duplicate field ids for ${property.definition}.$property '''
              '''and ${other.definition}.$other.''',
              front: Styles.RED);
          runGenerator = false;
        } else if (property.key!.intValue! < minPropertyId) {
          color(
              '${property.definition}.$property: ids less than '
              '$minPropertyId are reserved.',
              front: Styles.RED);
          runGenerator = false;
        } else {
          properties[property.key!.intValue!] = property;
        }
      }
    }

    // Find max id, we use this to assign to types that don't have ids yet.
    int nextFieldId = minPropertyId - 1;
    int nextId = 0;
    for (final definition in definitions.values) {
      var intValue = definition._key?.intValue;
      if (intValue != null && intValue > nextId) {
        nextId = intValue;
      }
      for (final field in definition._properties) {
        var intValue = field.key?.intValue;
        if (intValue != null && intValue > nextFieldId) {
          nextFieldId = intValue;
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

    StringBuffer ctxCode = StringBuffer('');
    var includes = <String>{};
    var runtimeDefinitions =
        definitions.values.where((definition) => definition.forRuntime);
    for (final definition in runtimeDefinitions) {
      includes.add(definition.concreteCodeFilename);
    }
    var includeList = includes.toList()..sort();
    for (final include in includeList) {
      ctxCode.writeln('#include "$include"');
    }
    ctxCode.writeln('namespace rive {class CoreRegistry {'
        'public:');
    ctxCode.writeln('static Core* makeCoreInstance(int typeKey) {'
        'switch(typeKey) {');
    for (final definition in runtimeDefinitions) {
      if (definition._isAbstract) {
        continue;
      }
      ctxCode.writeln('case ${definition.name}Base::typeKey:');
      ctxCode.writeln('return new ${definition.name}();');
    }
    ctxCode.writeln('} return nullptr; }');

    var usedFieldTypes = <FieldType, List<Property>>{};
    var getSetFieldTypes = <FieldType, List<Property>>{};
    for (final definition in runtimeDefinitions) {
      for (final property in definition.properties) {
        usedFieldTypes[property.type] ??= [];
        usedFieldTypes[property.type]!.add(property);
        if (!property.isEncoded) {
          getSetFieldTypes[property.type] ??= [];
          getSetFieldTypes[property.type]!.add(property);
        }
      }
    }
    for (final fieldType in getSetFieldTypes.keys) {
      ctxCode
          .writeln('static void set${fieldType.capitalizedName}(Core* object, '
              'int propertyKey, ${fieldType.cppName} value){');
      ctxCode.writeln('switch (propertyKey) {');
      var properties = getSetFieldTypes[fieldType];
      if (properties != null) {
        for (final property in properties) {
          ctxCode.writeln('case ${property.definition.name}Base'
              '::${property.name}PropertyKey:');
          ctxCode.writeln('object->as<${property.definition.name}Base>()->'
              '${property.name}(value);');
          ctxCode.writeln('break;');
        }
      }
      ctxCode.writeln('}}');
    }
    for (final fieldType in getSetFieldTypes.keys) {
      if (!fieldType.storesData) {
        continue;
      }
      ctxCode.writeln(
          'static ${fieldType.cppName} get${fieldType.capitalizedName}('
          'Core* object, int propertyKey){');
      ctxCode.writeln('switch (propertyKey) {');
      var properties = getSetFieldTypes[fieldType];
      if (properties != null) {
        for (final property in properties) {
          ctxCode.writeln('case ${property.definition.name}Base'
              '::${property.name}PropertyKey:');
          ctxCode
              .writeln('return object->as<${property.definition.name}Base>()->'
                  '${property.name}();');
        }
      }
      ctxCode.writeln('}');
      ctxCode.writeln('return ${fieldType.defaultValue ?? 'nullptr'};');
      ctxCode.writeln('}');
    }

    ctxCode.writeln('static int propertyFieldId(int propertyKey) {');
    ctxCode.writeln('switch(propertyKey) {');

    for (final fieldType in usedFieldTypes.keys) {
      if (!fieldType.storesData) {
        continue;
      }
      var properties = usedFieldTypes[fieldType];
      if (properties != null) {
        for (final property in properties) {
          ctxCode.writeln('case ${property.definition.name}Base'
              '::${property.name}PropertyKey:');
        }
      }
      ctxCode.writeln('return Core${fieldType.capitalizedName}Type::id;');
    }

    ctxCode.writeln('default: return -1;}}');

    ctxCode.writeln('''
      static bool isCallback(uint32_t propertyKey) {
        switch(propertyKey) {''');
    for (final fieldType in usedFieldTypes.keys) {
      var properties = usedFieldTypes[fieldType];
      if (properties != null) {
        bool found = false;
        for (final property in properties) {
          if (property.getExportType().name == 'callback') {
            found = true;
            ctxCode.write('case ${property.definition._name}Base');
            ctxCode.write('::${property.name}PropertyKey:');
          }
        }
        if (found) {
          ctxCode.writeln('return true;');
        }
      }
    }
    ctxCode.writeln('default:return false;');
    ctxCode.writeln('}}');

    ctxCode.writeln('};}');

    var output = generatedHppPath;
    var folder = output.isNotEmpty && output[output.length - 1] == '/'
        ? output.substring(0, output.length - 1)
        : output;

    var file = File('$folder/core_registry.hpp');
    file.createSync(recursive: true);

    var formattedCode =
        await _formatter.formatAndGuard('CoreRegistry', ctxCode.toString());
    file.writeAsStringSync(formattedCode, flush: true);

    return true;
  }
}

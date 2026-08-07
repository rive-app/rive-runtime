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

String get withRiveToolsPreprocessor => 'WITH_RIVE_TOOLS';
void addPreprocessorStart(StringBuffer buffer, String def) {
  buffer.writeln('#ifdef ' + def);
}

void addPreprocessorEnd(StringBuffer buffer) {
  buffer.writeln('#endif');
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
  Iterable<Property> get storedPropertiesNoPassthrough =>
      properties.where((property) =>
          property.getExportType().storesData &&
          !property.isPassthrough &&
          !property.isBitmaskPassthrough);

  /// Runtime-only clusters of "cold" properties hoisted into lazily-allocated
  /// sidecar structs (see [Property.sidecarName]). Insertion-ordered:
  /// sidecar name -> properties in that cluster. C++ generator only.
  Map<String, List<Property>> get sidecarGroups {
    final groups = <String, List<Property>>{};
    for (final property in properties) {
      if (property.isSidecar) {
        groups.putIfAbsent(property.sidecarName!, () => []).add(property);
      }
    }
    return groups;
  }

  /// Sidecar storage packs plain stored fields into a POD struct behind a lazy
  /// pointer; it is incompatible with any property whose storage/dispatch is
  /// special-cased. Fail loudly at generation rather than emit broken C++.
  void _validateSidecars() {
    for (final property in properties) {
      if (!property.isSidecar) {
        continue;
      }
      if (!property.getExportType().storesData ||
          property.isEncoded ||
          property.isPassthrough ||
          property.isBitmaskPassthrough ||
          property.isVirtual ||
          property.isPureVirtual ||
          property.isGetOverride ||
          property.isSetOverride) {
        throw Exception(
            'Property ${_name}.${property.name} cannot be a sidecar: sidecar '
            'is only supported for plain stored (non-virtual, non-override, '
            'non-encoded, non-passthrough) properties.');
      }
    }
  }

  String _sidecarStructName(Property property) =>
      '${_name}${property.capitalizedSidecarName}Sidecar';

  Definition? _extensionOf;
  Definition? _rawExtensionOf;
  Key? _key;
  bool _isAbstract = false;
  bool _isMixin = false;
  bool get isMixin => _isMixin;
  final List<Definition> _mixinsOf = [];
  List<Definition> get mixinsOf => _mixinsOf;
  bool _editorOnly = false;
  bool _forRuntime = true;
  bool get forRuntime => _forRuntime;

  Definition? getRuntimeExtensionOf(Definition? definition) {
    var extensionOf = definition;
    if (extensionOf != null) {
      if (extensionOf._forRuntime) {
        return extensionOf;
      }
      return getRuntimeExtensionOf(extensionOf._extensionOf);
    }
    return extensionOf;
  }

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
      _rawExtensionOf = Definition.make(extendsFilename);
      _extensionOf = getRuntimeExtensionOf(_rawExtensionOf);
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
    dynamic isMixinValue = data['isMixin'];
    if (isMixinValue is bool) {
      _isMixin = isMixinValue;
    }
    // Parse mixins. In the C++ runtime a mixin is emitted as a non-Core
    // interface base (multiple inheritance) plus per-consumer dispatch in
    // core_registry (via a static from(Core*) resolver). We record the mixin
    // relationship for codegen/registry dispatch, but do not fold the mixin's
    // properties into the consumer's own property list.
    void addMixin(String name) {
      // Editor-only mixins (e.g. publishable, taggable) are not synced into the
      // runtime def tree. Skip any mixin whose def is absent here — only
      // runtime-capable mixins (e.g. color_channels) participate in C++.
      if (!File(defsPath + name).existsSync()) {
        return;
      }
      final mixinDef = Definition.make(name);
      if (mixinDef != null) {
        _mixinsOf.add(mixinDef);
      }
    }

    dynamic mixinFilename = data['mixin'];
    if (mixinFilename is String) {
      addMixin(mixinFilename);
    }
    dynamic mixinFilenames = data['mixins'];
    if (mixinFilenames is List) {
      for (final name in mixinFilenames) {
        if (name is String) {
          addMixin(name);
        }
      }
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
    _validateBitmaskPassthroughs();
  }

  void _validateBitmaskPassthroughs() {
    for (final p in _properties) {
      if (!p.isBitmaskPassthrough) {
        continue;
      }
      if (p.isPassthrough) {
        color(
          '${p.name}: cannot use passthrough with passthroughForBitmask.',
          front: Styles.RED,
        );
      }
      final isBool = p.type.name == 'bool';
      final isUint = p.type.name == 'uint';
      if (!isBool && !isUint) {
        color(
          '${p.name}: passthroughForBitmask requires bool or uint.',
          front: Styles.RED,
        );
      }
      if (isBool &&
          p.passthroughBitWidth != null &&
          p.passthroughBitWidth != 1) {
        color(
          '${p.name}: bool passthroughForBitmask must have width 1.',
          front: Styles.RED,
        );
      }
      if (isUint &&
          (p.passthroughBitWidth == null || p.passthroughBitWidth! < 1)) {
        color(
          '${p.name}: uint passthroughForBitmask requires '
          'passthroughBitWidth >= 1.',
          front: Styles.RED,
        );
      }
      Property? target;
      for (final q in _properties) {
        if (q.name == p.passthroughForBitmask) {
          target = q;
          break;
        }
      }
      if (target == null) {
        // A mixin can passthrough a mask provided by the host class that
        // includes it (e.g. `colorValue` on SolidColor/GradientStop). The
        // mask is not a same-def sibling; core_registry drives it per
        // consuming type. Bit/width validation still applies.
        if (_isMixin) {
          p.bitmaskTargetIsHostProvided = true;
          final bit = p.passthroughBit!;
          final width = p.passthroughBitWidthOrDefault;
          if (bit < 0 || width < 1 || bit + width > 32) {
            color(
              '${p.name}: passthroughBit/passthroughBitWidth must fit in 0..32 '
              '(bit $bit, width $width).',
              front: Styles.RED,
            );
          }
          continue;
        }
        color(
          '${p.name}: passthroughForBitmask "${p.passthroughForBitmask}" '
          'not found.',
          front: Styles.RED,
        );
        continue;
      }
      // Color masks are uint32 under the hood, so they are valid targets too.
      if (target.type.name != 'uint' && target.type.name != 'Color') {
        color(
          '${p.name}: passthroughForBitmask target must be uint or Color.',
          front: Styles.RED,
        );
        continue;
      }
      if (target.isEncoded || target.isPassthrough) {
        color(
          '${p.name}: invalid passthroughForBitmask target.',
          front: Styles.RED,
        );
        continue;
      }
      final bit = p.passthroughBit!;
      final width = p.passthroughBitWidthOrDefault;
      if (bit < 0 || width < 1 || bit + width > 32) {
        color(
          '${p.name}: passthroughBit/passthroughBitWidth must fit in 0..32 '
          '(bit $bit, width $width).',
          front: Styles.RED,
        );
        continue;
      }
      p.bitmaskTargetProperty = target;
    }
    _validateBitmaskPassthroughOverlaps();
  }

  /// Ensure no two bitmask passthroughs targeting the same mask occupy
  /// overlapping bit ranges.
  void _validateBitmaskPassthroughOverlaps() {
    final byMask = <String, List<Property>>{};
    for (final p in _properties) {
      if (p.isBitmaskPassthrough && p.bitmaskTargetProperty != null) {
        (byMask[p.passthroughForBitmask!] ??= []).add(p);
      }
    }
    byMask.forEach((mask, props) {
      for (var i = 0; i < props.length; i++) {
        for (var j = i + 1; j < props.length; j++) {
          final a = props[i];
          final b = props[j];
          final aStart = a.passthroughBit!;
          final aEnd = aStart + a.passthroughBitWidthOrDefault;
          final bStart = b.passthroughBit!;
          final bEnd = bStart + b.passthroughBitWidthOrDefault;
          if (aStart < bEnd && bStart < aEnd) {
            color(
              '${a.name} and ${b.name}: overlapping passthrough bit ranges '
              'on $mask ([$aStart,$aEnd) vs [$bStart,$bEnd)).',
              front: Styles.RED,
            );
          }
        }
      }
    });
  }

  String get localFilename => _filename.indexOf(defsPath) == 0
      ? _filename.substring(defsPath.length)
      : _filename;

  String? get name => _name;

  String get localCodeFilename => '${stripExtension(_filename)}_base.hpp';
  String get concreteCodeFilename => 'rive/${stripExtension(_filename)}.hpp';
  String get localCppCodeFilename => '${stripExtension(_filename)}_base.cpp';

  /// Runtime types that include this mixin (scanned once all defs are loaded).
  List<Definition> get _mixinConsumers => definitions.values
      .where((d) => d.forRuntime && d._mixinsOf.contains(this))
      .toList();

  /// A runtime mixin is emitted as a non-Core interface class (like
  /// [ListConstraint]): it declares the host mask(s) as pure virtuals — which
  /// the consuming type's own generated accessor satisfies — and provides the
  /// shared channel accessor methods + key constants. A static [from] resolves
  /// a Core object to the interface (or nullptr). Consuming types inherit it as
  /// a second base, and core_registry dispatches the shared keys through
  /// [from].
  Future<void> _generateMixinInterfaceHeader() async {
    // Distinct host-provided masks required by this mixin's passthroughs.
    final hostMasks = <String>{};
    for (final property in properties) {
      if (property.isBitmaskPassthrough &&
          property.bitmaskTargetIsHostProvided) {
        hostMasks.add(property.passthroughForBitmask!);
      }
    }

    StringBuffer code = StringBuffer();
    code.writeln('#include <cstdint>');
    code.writeln('namespace rive {');
    code.writeln('class Core;');
    code.writeln('class ${_name}Base {');
    code.writeln('public:');
    code.writeln('static ${_name}Base* from(Core* object);');
    // The host provides the packed mask; its own accessor overrides these.
    for (final mask in hostMasks) {
      code.writeln('virtual int $mask() const = 0;');
      code.writeln('virtual void $mask(int value) = 0;');
    }
    for (final property in properties) {
      code.writeln('static const uint16_t ${property.name}PropertyKey = '
          '${property.key!.intValue};');
      for (final altKey in property.key!.alternates) {
        code.writeln('static const uint16_t ${altKey.stringValue}PropertyKey = '
            '${altKey.intValue};');
      }
      if (property.isBitmaskPassthrough &&
          property.bitmaskTargetIsHostProvided &&
          property.type.name == 'uint') {
        final mask = property.passthroughForBitmask!;
        final bit = property.passthroughBit!;
        final width = property.passthroughBitWidthOrDefault;
        final fieldMask = ((1 << width) - 1) << bit;
        final valueMask = (1 << width) - 1;
        code.writeln('static const uint32_t ${property.name}BitOffset = $bit;');
        code.writeln('static const uint32_t ${property.name}FieldMask = '
            '${fieldMask}u;');
        code.writeln('uint32_t ${property.name}() const { return '
            '(static_cast<uint32_t>($mask()) >> $bit) & ${valueMask}u; }');
        code.writeln('void ${property.name}(uint32_t value) {');
        // Clamp to the field's range so a channel saturates instead of
        // wrapping into the neighbouring byte.
        code.writeln('if (value > ${valueMask}u) { value = ${valueMask}u; }');
        code.writeln('const int _cur = $mask();');
        code.writeln('const int _fieldMask = static_cast<int>(${fieldMask}u);');
        code.writeln('const int _next = static_cast<int>('
            '(_cur & ~_fieldMask) | ((value << $bit) & _fieldMask));');
        code.writeln('if (_cur != _next) { $mask(_next); }');
        code.writeln('}');
      }
    }
    code.writeln('};');
    code.writeln('}');

    var file = File('$generatedHppPath$localCodeFilename');
    file.createSync(recursive: true);
    var formattedCode =
        await _formatter.formatAndGuard('${_name}Base', code.toString());
    file.writeAsStringSync(formattedCode, flush: true);

    // Emit the from() implementation switching over consuming types.
    final consumers = _mixinConsumers;
    StringBuffer cpp = StringBuffer();
    cpp.writeln('#include "rive/generated/$localCodeFilename"');
    cpp.writeln('#include "rive/core.hpp"');
    for (final consumer in consumers) {
      cpp.writeln('#include "${consumer.concreteCodeFilename}"');
    }
    cpp.writeln('using namespace rive;');
    cpp.writeln('${_name}Base* ${_name}Base::from(Core* object) {');
    cpp.writeln('switch (object->coreType()) {');
    for (final consumer in consumers) {
      cpp.writeln('case ${consumer.name}Base::typeKey:');
      cpp.writeln('return object->as<${consumer.name}Base>();');
    }
    cpp.writeln('} return nullptr; }');

    var cppFile = File('$generatedCppPath$localCppCodeFilename');
    cppFile.createSync(recursive: true);
    var formattedCpp = await _formatter.format(cpp.toString());
    cppFile.writeAsStringSync(formattedCpp, flush: true);
  }

  /// Generates cpp header code based on the Definition
  Future<void> generateCode() async {
    if (!_forRuntime) {
      return;
    }
    if (_isMixin) {
      await _generateMixinInterfaceHeader();
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
    // Runtime mixins are inherited as (non-Core) second bases.
    for (final mixin in _mixinsOf) {
      includes.add('rive/generated/${mixin.localCodeFilename}');
    }
    final sidecars = sidecarGroups;
    if (sidecars.isNotEmpty) {
      _validateSidecars();
      includes.add('rive/sidecar.hpp');
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
    // Sidecar payload structs: one POD per cluster, emitted before the class so
    // the lazy holder member and inline accessors see the complete type.
    for (final group in sidecars.values) {
      final structName = _sidecarStructName(group.first);
      code.writeln('struct $structName {');
      for (final property in group) {
        var initialize = property.initialValueRuntime ??
            property.initialValue ??
            property.type.defaultValue;
        String fieldLine = '${property.type.cppName} ${property.name}';
        if (initialize != null) {
          var converted = property.type.convertCpp(initialize);
          if (converted != null) {
            fieldLine += ' = $converted';
          }
        }
        code.writeln('$fieldLine;');
      }
      code.writeln('};');
    }
    var superTypeName = defineContextExtension ? 'Core' : _extensionOf?._name;
    final mixinBases =
        _mixinsOf.map((mixin) => ', public ${mixin.name}Base').join();
    code.writeln('class ${_name}Base : public $superTypeName$mixinBases {');

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
        if (property.isWithRiveToolsOnly) {
          addPreprocessorStart(code, withRiveToolsPreprocessor);
        }
        code.writeln('static const uint16_t ${property.name}PropertyKey = '
            '${property.key!.intValue};');
        for (final altKey in property.key!.alternates) {
          code.writeln(
              'static const uint16_t ${altKey.stringValue}PropertyKey = '
              '${altKey.intValue};');
        }
        // Bitmask passthroughs expose the authoritative bit layout as
        // compile-time constants so adapters on both sides consume the same
        // single source of truth (the JSON's passthroughBit/Width). Bools
        // keep the legacy single-bit `${name}Bitmask`; uints expose the
        // field offset and mask.
        if (property.isBitmaskPassthrough) {
          if (property.type.name == 'uint') {
            final width = property.passthroughBitWidthOrDefault;
            final fieldMask = ((1 << width) - 1) << property.passthroughBit!;
            code.writeln('static const uint32_t ${property.name}BitOffset = '
                '${property.passthroughBit};');
            code.writeln('static const uint32_t ${property.name}FieldMask = '
                '${fieldMask}u;');
          } else {
            code.writeln('static const uint32_t ${property.name}Bitmask = '
                '1u << ${property.passthroughBit};');
          }
        }
        if (property.isWithRiveToolsOnly) {
          addPreprocessorEnd(code);
          code.writeln('\n');
        }
      }
      if (storedProperties.any((prop) => !prop.isEncoded)) {
        code.writeln('protected:');
      }

      // Write fields.
      for (final property in properties) {
        if (property.isEncoded ||
            !property.getExportType().storesData ||
            property.isPassthrough ||
            property.isBitmaskPassthrough ||
            property.isSidecar) {
          // Encoded properties don't store data, it's up to the implementation
          // to decode and store what it needs. Sidecar properties are stored in
          // a lazily-allocated holder emitted below, not as an inline field.
          continue;
        }
        if (property.isWithRiveToolsOnly) {
          addPreprocessorStart(code, withRiveToolsPreprocessor);
        }
        // Emit the field as a single line to avoid trailing text before
        // preprocessor directives (e.g. '#endif').
        var initialize = property.initialValueRuntime ??
            property.initialValue ??
            property.type.defaultValue;
        String fieldLine =
            '${property.type.cppName} m_${property.capitalizedName}';
        if (initialize != null) {
          var converted = property.type.convertCpp(initialize);
          if (converted != null) {
            fieldLine += ' = $converted';
          }
        }
        code.writeln('$fieldLine;');
        if (property.isWithRiveToolsOnly) {
          addPreprocessorEnd(code);
          code.writeln('\n');
        }
      }

      // Lazily-allocated sidecar holders: one 8-byte pointer per cluster, null
      // until a property in the cluster is authored.
      for (final entry in sidecars.entries) {
        final structName = _sidecarStructName(entry.value.first);
        code.writeln('Sidecar<$structName> m_${entry.key};');
      }

      // Write getter/setters.
      code.writeln('public:');
      for (final property in properties) {
        if (property.isWithRiveToolsOnly) {
          addPreprocessorStart(code, withRiveToolsPreprocessor);
        }
        if (property.isBitmaskPassthrough) {
          continue;
        }
        // A stored mask property (e.g. colorValue) that satisfies a mixin's
        // host-provided pure virtual must be marked `override`.
        final overridesMixinMask = _mixinsOf.any((m) => m.properties.any((mp) =>
            mp.bitmaskTargetIsHostProvided &&
            mp.passthroughForBitmask == property.name));
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
        } else if (property.isPassthrough) {
          code.writeln('virtual void set${property.capitalizedName}('
              '${property.type.cppGetterName} value) = 0;');
          code.writeln(
              'virtual ${property.type.cppGetterName} ${property.name}() '
              '= 0;');
          code.writeln(
              'void ${property.name}(${property.type.cppName} value) ' +
                  (property.isSetOverride ? 'override' : '') +
                  '{'
                      'if(${property.name}() == value)'
                      '{return;}'
                      'set${property.capitalizedName}(value);'
                      '${property.name}Changed();'
                      'notifyPropertyChanged(${property.name}PropertyKey);'
                      '}');
        } else if (property.isSidecar) {
          // Getter falls back to the compile-time default when the cluster was
          // never allocated; setter allocates on the first non-default write.
          final memberName = 'm_${property.sidecarName}';
          var initialize = property.initialValueRuntime ??
              property.initialValue ??
              property.type.defaultValue;
          var converted =
              initialize != null ? property.type.convertCpp(initialize) : null;
          final getterType =
              (property.type.cppGetterName ?? property.type.cppName)!;
          if (getterType.trimRight().endsWith('&')) {
            // By-reference getter (e.g. `const std::string&`): the null branch
            // must return an lvalue of the same type, otherwise the ternary
            // materializes a temporary and the returned reference dangles. Hold
            // the default in a function-local static of the value type.
            final defaultLiteral = converted ?? '${property.type.cppName}{}';
            code.writeln('inline $getterType ${property.name}() const {'
                'static const ${property.type.cppName} defaultValue = '
                '$defaultLiteral;'
                'auto* sidecar = $memberName.get();'
                'return sidecar != nullptr ? sidecar->${property.name} : '
                'defaultValue; }');
          } else {
            final defaultLiteral = converted ?? '$getterType{}';
            code.writeln('inline $getterType '
                '${property.name}() const { auto* sidecar = $memberName.get(); '
                'return sidecar != nullptr ? sidecar->${property.name} : '
                '$defaultLiteral; }');
          }
          code.writeln('void ${property.name}(${property.type.cppName} value) {'
              'if(${property.name}() == value){return;}'
              '$memberName.ensure()->${property.name} = value;'
              '${property.name}Changed();'
              'notifyPropertyChanged(${property.name}PropertyKey);'
              '}');
        } else {
          code.writeln(((property.isVirtual || property.isPureVirtual)
                  ? 'virtual'
                  : 'inline') +
              ' ${property.type.cppGetterName} ${property.name}() const ' +
              ((property.isGetOverride || overridesMixinMask)
                  ? 'override'
                  : '') +
              '{ return m_${property.capitalizedName}; }');
          if (!property.isPureVirtual) {
            code.writeln(
                'void ${property.name}(${property.type.cppName} value) ' +
                    ((property.isSetOverride || overridesMixinMask)
                        ? 'override'
                        : '') +
                    '{'
                        'if(m_${property.capitalizedName} == value)'
                        '{return;}'
                        'm_${property.capitalizedName} = value;'
                        '${property.name}Changed();'
                        'notifyPropertyChanged(${property.name}PropertyKey);'
                        '}');
          } else {
            code.writeln(
                '''virtual void ${property.name}(${property.type.cppName} value) = 0;''');
          }
        }
        if (property.isWithRiveToolsOnly) {
          addPreprocessorEnd(code);
        }

        code.writeln();
      }
    }

    if (!_isAbstract) {
      code.writeln('Core* clone() const override;');
    }

    if (storedPropertiesNoPassthrough.isNotEmpty || _extensionOf == null) {
      code.writeln('void copy(const ${_name}Base& object) {');
      for (final property in storedPropertiesNoPassthrough) {
        if (property.isSidecar) {
          // Copied once per cluster below via the holder's deep copy.
          continue;
        }
        if (property.isWithRiveToolsOnly) {
          addPreprocessorStart(code, withRiveToolsPreprocessor);
        }
        if (property.isEncoded) {
          code.writeln('copy${property.capitalizedName}(object);');
        } else {
          code.writeln('m_${property.capitalizedName} = '
              'object.m_${property.capitalizedName};');
        }
        if (property.isWithRiveToolsOnly) {
          addPreprocessorEnd(code);
        }
      }
      // Deep-copy each sidecar cluster (Sidecar<T> handles null vs allocated).
      for (final entry in sidecars.entries) {
        code.writeln('m_${entry.key} = object.m_${entry.key};');
      }
      if (_extensionOf != null) {
        code.writeln('${_extensionOf!.name}::'
            'copy(object); ');
      }
      code.writeln('}');
      code.writeln();

      code.writeln('bool deserialize(uint16_t propertyKey, '
          'BinaryReader& reader) override {');

      if (storedPropertiesNoPassthrough.isNotEmpty) {
        code.writeln('switch (propertyKey){');
        for (final property in storedPropertiesNoPassthrough) {
          if (property.isWithRiveToolsOnly) {
            addPreprocessorStart(code, withRiveToolsPreprocessor);
          }
          code.writeln('case ${property.name}PropertyKey:');
          if (property.isSidecar) {
            code.writeln('m_${property.sidecarName}.ensure()->${property.name} '
                '= ${property.type.runtimeCoreType}::deserialize(reader);');
          } else if (property.isEncoded) {
            code.writeln('decode${property.capitalizedName}'
                '(${property.type.runtimeCoreType}::deserialize(reader));');
          } else {
            code.writeln('m_${property.capitalizedName} = '
                '${property.type.runtimeCoreType}::deserialize(reader);');
          }
          code.writeln('return true;');
          if (property.isWithRiveToolsOnly) {
            addPreprocessorEnd(code);
          }
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
        if (property.isBitmaskPassthrough) {
          continue;
        }
        if (property.isWithRiveToolsOnly) {
          addPreprocessorStart(code, withRiveToolsPreprocessor);
        }
        code.writeln('virtual void ${property.name}Changed() {}');
        if (property.isWithRiveToolsOnly) {
          addPreprocessorEnd(code);
        }
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
      // Mixins have no concrete class; include their (constants-only) base
      // header directly so core_registry can reference the shared key
      // constants.
      includes.add(definition._isMixin
          ? 'rive/generated/${definition.localCodeFilename}'
          : definition.concreteCodeFilename);
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
        // Group by registryType so narrow aliases (e.g. uint8) fold into their
        // wider type (uint): set/get dispatch and the property field-type id
        // stay shared, keeping existing keyframes and files compatible.
        final registryType = property.type.registryType;
        usedFieldTypes[registryType] ??= [];
        usedFieldTypes[registryType]!.add(property);
        if (!property.isEncoded) {
          getSetFieldTypes[registryType] ??= [];
          getSetFieldTypes[registryType]!.add(property);
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
          if (property.isWithRiveToolsOnly) {
            addPreprocessorStart(ctxCode, withRiveToolsPreprocessor);
          }
          if (property.isBitmaskPassthrough &&
              property.bitmaskTargetIsHostProvided) {
            // Shared channel defined in a mixin. Resolve the interface via
            // from() and let it do the masked write on the host's mask.
            final iface = '${property.definition.name}Base';
            ctxCode.writeln('case $iface::${property.name}PropertyKey:');
            for (final altKey in property.key!.alternates) {
              ctxCode.writeln('case $iface'
                  '::${altKey.stringValue}PropertyKey:');
            }
            ctxCode.writeln('{');
            ctxCode.writeln('if (auto* _c = $iface::from(object)) { '
                '_c->${property.name}(value); }');
            ctxCode.writeln('break;');
            ctxCode.writeln('}');
          } else if (property.isBitmaskPassthrough) {
            final mask = property.bitmaskTargetProperty!.name;
            final bit = property.passthroughBit!;
            final maskType = property.bitmaskTargetProperty!.type.cppName;
            final defName = property.definition.name;
            ctxCode.writeln('case ${defName}Base'
                '::${property.name}PropertyKey:');
            if (property.key != null) {
              for (final altKey in property.key!.alternates) {
                ctxCode.writeln('case ${defName}Base'
                    '::${altKey.stringValue}PropertyKey:');
              }
            }
            ctxCode.writeln('{');
            ctxCode.writeln('auto* _o = object->as<${defName}Base>();');
            ctxCode.writeln('if (_o) {');
            ctxCode.writeln('const $maskType _cur = _o->$mask();');
            if (property.type.name == 'uint') {
              final width = property.passthroughBitWidthOrDefault;
              final fieldMask = ((1 << width) - 1) << bit;
              ctxCode.writeln('const $maskType _fieldMask = '
                  'static_cast<$maskType>(${fieldMask}u);');
              ctxCode.writeln('const $maskType _next = static_cast<$maskType>(('
                  '_cur & ~_fieldMask) | ((value << $bit) & _fieldMask));');
            } else {
              ctxCode.writeln(
                  'const $maskType _bm = static_cast<$maskType>(1u << $bit);');
              ctxCode.writeln('const $maskType _next = static_cast<$maskType>(('
                  '_cur & ~_bm) | (value ? _bm : static_cast<$maskType>(0)));');
            }
            ctxCode.writeln('if (_cur != _next) { _o->$mask(_next); }');
            ctxCode.writeln('}');
            ctxCode.writeln('break;');
            ctxCode.writeln('}');
          } else {
            ctxCode.writeln('case ${property.definition.name}Base'
                '::${property.name}PropertyKey:');
            if (property.key != null) {
              for (final altKey in property.key!.alternates) {
                ctxCode.writeln('case ${property.definition.name}Base'
                    '::${altKey.stringValue}PropertyKey:');
              }
            }
            ctxCode.writeln('object->as<${property.definition.name}Base>()->'
                '${property.name}(value);');
            ctxCode.writeln('break;');
          }
          if (property.isWithRiveToolsOnly) {
            addPreprocessorEnd(ctxCode);
          }
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
          // Bool passthroughs have no read-back getter (mirrors the field
          // having no backing storage). uint passthroughs read their field
          // back out of the mask so runtime data binding can round-trip.
          if (property.isBitmaskPassthrough && property.type.name != 'uint') {
            continue;
          }
          if (property.isWithRiveToolsOnly) {
            addPreprocessorStart(ctxCode, withRiveToolsPreprocessor);
          }
          ctxCode.writeln('case ${property.definition.name}Base'
              '::${property.name}PropertyKey:');
          for (final altKey in property.key!.alternates) {
            ctxCode.writeln('case ${property.definition.name}Base'
                '::${altKey.stringValue}PropertyKey:');
          }
          if (property.isBitmaskPassthrough &&
              property.bitmaskTargetIsHostProvided) {
            final iface = '${property.definition.name}Base';
            ctxCode.writeln('if (auto* _c = $iface::from(object)) { '
                'return _c->${property.name}(); }');
            ctxCode.writeln('return 0u;');
          } else if (property.isBitmaskPassthrough) {
            final mask = property.bitmaskTargetProperty!.name;
            final bit = property.passthroughBit!;
            final width = property.passthroughBitWidthOrDefault;
            final valueMask = (1 << width) - 1;
            ctxCode.writeln(
                'return (object->as<${property.definition.name}Base>()->'
                '$mask() >> $bit) & ${valueMask}u;');
          } else {
            ctxCode.writeln(
                'return object->as<${property.definition.name}Base>()->'
                '${property.name}();');
          }
          if (property.isWithRiveToolsOnly) {
            addPreprocessorEnd(ctxCode);
          }
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
          // Bitmask passthroughs are not serialized on their own, but they DO
          // have a runtime field type (their value type, e.g. uint) that data
          // binding needs to resolve the target value — so include them here.
          if (property.isWithRiveToolsOnly) {
            addPreprocessorStart(ctxCode, withRiveToolsPreprocessor);
          }
          ctxCode.writeln('case ${property.definition.name}Base'
              '::${property.name}PropertyKey:');
          if (property.isWithRiveToolsOnly) {
            addPreprocessorEnd(ctxCode);
          }
          for (final altKey in property.key!.alternates) {
            ctxCode.writeln('case ${property.definition.name}Base'
                '::${altKey.stringValue}PropertyKey:');
          }
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

    // ignore: lines_longer_than_80_chars
    // static bool objectSupportsProperty(Core* object, uint32_t propertyKey) { return true; }
    ctxCode.writeln('''
      static bool objectSupportsProperty(Core* object, uint32_t propertyKey) {
        switch(propertyKey) {''');
    for (final fieldType in usedFieldTypes.keys) {
      var properties = getSetFieldTypes[fieldType];
      if (properties != null) {
        for (final property in properties) {
          if (property.isWithRiveToolsOnly) {
            addPreprocessorStart(ctxCode, withRiveToolsPreprocessor);
          }
          ctxCode.writeln('case ${property.definition.name}Base'
              '::${property.name}PropertyKey:');
          for (final altKey in property.key!.alternates) {
            ctxCode.writeln('case ${property.definition.name}Base'
                '::${altKey.stringValue}PropertyKey:');
          }
          if (property.bitmaskTargetIsHostProvided) {
            // Shared mixin key: supported by any consuming type.
            ctxCode
                .writeln('return ${property.definition.name}Base::from(object) '
                    '!= nullptr;');
          } else {
            ctxCode.writeln(
                'return object->is<${property.definition.name}Base>();');
          }
          if (property.isWithRiveToolsOnly) {
            addPreprocessorEnd(ctxCode);
          }
        }
      }
    }
    ctxCode.writeln('}return false;}');
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

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

/// Defined only by editor_native's premake. Gates hooks that make the object
/// model editable (change recording, coop serialization, arena/CoreRef). The
/// runtime (Flutter SDK, standalone consumers) never defines this, so these
/// hooks compile out to nothing and the shipped runtime has zero overhead.
String get withRiveEditorPreprocessor => 'WITH_RIVE_EDITOR';

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
    dynamic runtimeFlag = data['runtime'];
    if (runtimeFlag is bool) {
      _forRuntime = runtimeFlag;
    }
    dynamic extendsFilename = data['extends'];
    if (extendsFilename is String) {
      _rawExtensionOf = Definition.make(extendsFilename);
      // an editor-only class keeps its def parent; only runtime classes
      // skip past editor-only ancestors
      _extensionOf =
          _forRuntime ? getRuntimeExtensionOf(_rawExtensionOf) : _rawExtensionOf;
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
      if (!File(defsPath + name).existsSync()) {
        return;
      }
      final mixinDef = Definition.make(name);
      // Editor-only mixins (publishable, taggable, ...) carry no C++ storage:
      // the emitted interface is key constants plus a from() resolver, and
      // nothing can dispatch a plain property through it. Skip them so runtime
      // consumers keep a single base.
      if (mixinDef != null && mixinDef._forRuntime) {
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
      final isUint = p.type.registryType.name == 'uint';
      if (!isBool && !isUint) {
        color(
          '${p.name}: passthroughForBitmask requires bool, uint or uint8.',
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

  /// Immediate parent in the def tree (`extends` chain), `null` for
  /// roots. Exposed for the pump_cores dart emit which mirrors the
  /// def hierarchy. `_extensionOf` is the same getter the C++ emit
  /// reads internally; this just opens it up to sibling generator
  /// files.
  Definition? get extensionOf => _extensionOf;

  String get localCodeFilename => '${stripExtension(_filename)}_base.hpp';
  // `"runtime": false` classes live under `editor_native/`; runtime
  // classes under `rive/`. The path prefix here becomes the `#include`
  // path used by CoreRegistry + parent-class references, so the
  // choice propagates through the include graph automatically.
  String get _concreteIncludePrefix => _forRuntime ? 'rive/' : 'editor_native/';
  String get concreteCodeFilename =>
      '$_concreteIncludePrefix${stripExtension(_filename)}.hpp';
  // Mirror the prefix for the generated base include (used by the
  // concrete stub + the cpp file). Runtime: `rive/generated/...`.
  // Editor-only: `editor_native/generated/...`.
  String get _generatedIncludePrefix =>
      _forRuntime ? 'rive/generated/' : 'editor_native/generated/';
  String get generatedIncludeFilename =>
      '$_generatedIncludePrefix$localCodeFilename';
  String get localCppCodeFilename => '${stripExtension(_filename)}_base.cpp';

  /// Editor-only members, accessors and dispatch for a runtime type. The
  /// generated class body includes it behind one `WITH_RIVE_EDITOR` guard so
  /// `rive/generated` stays free of editor code.
  static String get editorFieldTypesIncludeFilename =>
      'editor_native/generated/editor_field_types.hpp';

  String get localEditorExtFilename => '${stripExtension(_filename)}_ext.inl';
  String get editorExtIncludeFilename =>
      'editor_native/generated/$localEditorExtFilename';

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
          property.type.registryType.name == 'uint') {
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

  // Filesystem-output roots. Picks between the runtime and editor_
  // native destination paths based on `_forRuntime`. Matches
  // `_concreteIncludePrefix` / `_generatedIncludePrefix`.
  String get _outputGeneratedHppPath =>
      _forRuntime ? generatedHppPath : editorGeneratedHppPath;
  String get _outputConcreteHppPath =>
      _forRuntime ? concreteHppPath : editorConcreteHppPath;
  String get _outputGeneratedCppPath =>
      _forRuntime ? generatedCppPath : editorGeneratedCppPath;

  /// Writes the class-body `.inl` holding this type's editor-only members,
  /// accessors and dispatch. Section order matches the def order; the file is
  /// only ever included from inside the generated class.
  Future<void> _writeEditorExtension({
    required StringBuffer keys,
    required StringBuffer fields,
    required StringBuffer accessors,
    required StringBuffer copy,
    required StringBuffer deserialize,
    required StringBuffer methods,
    required StringBuffer changed,
  }) async {
    StringBuffer ext = StringBuffer();
    if (keys.isNotEmpty) {
      ext.writeln('public:');
      ext.write(keys);
    }
    if (fields.isNotEmpty) {
      ext.writeln('protected:');
      ext.write(fields);
    }
    if (accessors.isNotEmpty || copy.isNotEmpty || deserialize.isNotEmpty ||
        methods.isNotEmpty) {
      ext.writeln('public:');
    }
    ext.write(accessors);
    if (copy.isNotEmpty) {
      ext.writeln('void copyEditorProperties(const ${_name}Base& object) {');
      ext.write(copy);
      ext.writeln('}');
    }
    if (deserialize.isNotEmpty) {
      ext.writeln('bool deserializeEditorProperties(uint16_t propertyKey, '
          'BinaryReader& reader) {');
      ext.writeln('switch (propertyKey){');
      ext.write(deserialize);
      ext.writeln('}');
      ext.writeln('return false; }');
    }
    ext.write(methods);
    if (changed.isNotEmpty) {
      ext.writeln('protected:');
      ext.write(changed);
    }

    var file = File('$editorGeneratedHppPath$localEditorExtFilename');
    file.createSync(recursive: true);
    file.writeAsStringSync(await _formatter.formatClassBody(ext.toString()),
        flush: true);
  }

  /// Generates cpp header code based on the Definition
  Future<void> generateCode() async {
    // `"runtime": false` types are editor-only: the base is emitted under the
    // editor kernel, wrapped in `#ifdef WITH_RIVE_EDITOR`, so the runtime C++
    // build never sees the class.
    final bool editorOnly = !_forRuntime;

    // A runtime type's editor-only artifacts move into a class-body `.inl`
    // under the editor kernel; an editor-only type is wholly generated there
    // already, so it keeps everything in one file.
    final bool splitEditorExt = _forRuntime;

    void startPropertyGuards(StringBuffer buf, Property property) {
      if (property.isWithRiveToolsOnly) {
        addPreprocessorStart(buf, withRiveToolsPreprocessor);
      }
    }

    void endPropertyGuards(StringBuffer buf, Property property) {
      if (property.isWithRiveToolsOnly) {
        addPreprocessorEnd(buf);
      }
    }

    bool toExt(Property property) =>
        splitEditorExt && property.isWithRiveEditorOnly;
    final extKeys = StringBuffer();
    final extFields = StringBuffer();
    final extAccessors = StringBuffer();
    final extCopy = StringBuffer();
    final extDeserialize = StringBuffer();
    final extMethods = StringBuffer();
    final extChanged = StringBuffer();
    // applyChange and captureStateForJournal ride along whenever the shared
    // copy/deserialize bodies are emitted, so the extension exists for any
    // stored type even when no single property is editor-only.
    final bool hasEditorExt = splitEditorExt &&
        (properties.any(toExt) ||
            storedPropertiesNoPassthrough.isNotEmpty ||
            _extensionOf == null);

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
      if (toExt(property)) {
        // The extension's field types come in through the one shared editor
        // header below; the runtime include list stays minimal.
        continue;
      }
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
    // Only an extension that carries editor-only properties needs the extra
    // field types; one that just holds applyChange reuses what the runtime
    // properties already included.
    if (properties.any(toExt)) {
      addPreprocessorStart(code, withRiveEditorPreprocessor);
      code.writeln('#include "$editorFieldTypesIncludeFilename"');
      addPreprocessorEnd(code);
    }
    // Sidecar storage only exists in runtime builds; editor builds keep
    // full members so the change hooks see stable field addresses.
    if (sidecars.isNotEmpty) {
      code.writeln('#ifndef $withRiveEditorPreprocessor');
      code.writeln('#include "rive/sidecar.hpp"');
      code.writeln('#endif');
    }

    if (editorOnly) {
      addPreprocessorStart(code, withRiveEditorPreprocessor);
    }
    code.writeln('namespace rive {');
    // Sidecar payload structs: one POD per cluster, emitted before the class so
    // the lazy holder member and inline accessors see the complete type.
    if (sidecars.isNotEmpty) {
      code.writeln('#ifndef $withRiveEditorPreprocessor');
    }
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
    if (sidecars.isNotEmpty) {
      code.writeln('#endif');
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
        final keyBuf = toExt(property) ? extKeys : code;
        startPropertyGuards(keyBuf, property);
        keyBuf.writeln('static const uint16_t ${property.name}PropertyKey = '
            '${property.key!.intValue};');
        for (final altKey in property.key!.alternates) {
          keyBuf.writeln(
              'static const uint16_t ${altKey.stringValue}PropertyKey = '
              '${altKey.intValue};');
        }
        // Bitmask passthroughs expose the authoritative bit layout as
        // compile-time constants so adapters on both sides consume the same
        // single source of truth (the JSON's passthroughBit/Width). Bools
        // keep the legacy single-bit `${name}Bitmask`; uints expose the
        // field offset and mask.
        if (property.isBitmaskPassthrough) {
          if (property.type.registryType.name == 'uint') {
            final width = property.passthroughBitWidthOrDefault;
            final fieldMask = ((1 << width) - 1) << property.passthroughBit!;
            keyBuf.writeln('static const uint32_t ${property.name}BitOffset = '
                '${property.passthroughBit};');
            keyBuf.writeln('static const uint32_t ${property.name}FieldMask = '
                '${fieldMask}u;');
          } else {
            keyBuf.writeln('static const uint32_t ${property.name}Bitmask = '
                '1u << ${property.passthroughBit};');
          }
        }
        endPropertyGuards(keyBuf, property);
      }
      if (storedProperties.any((prop) => !prop.isEncoded && !toExt(prop))) {
        code.writeln('protected:');
      }

      // Write fields.
      for (final property in properties) {
        if (property.isEncoded ||
            !property.getExportType().storesData ||
            property.isPassthrough ||
            property.isBitmaskPassthrough) {
          // Encoded properties don't store data, it's up to the implementation
          // to decode and store what it needs.
          continue;
        }
        final fieldBuf = toExt(property) ? extFields : code;
        startPropertyGuards(fieldBuf, property);
        if (property.isSidecar) {
          // Editor builds keep the full member (hooks need stable field
          // addresses); runtime builds use the cluster holder below.
          addPreprocessorStart(fieldBuf, withRiveEditorPreprocessor);
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
        fieldBuf.writeln('$fieldLine;');
        // Animation-overlay storage. Mirrors Dart's
        // `_propertyValueAnimated` (rive_core *_base.dart): a separate
        // field that animation/data-binding writes to without touching
        // the persistent (static) value. Composed getter returns the
        // override when set, otherwise the static field. Reset path
        // (`<name>ResetOverride`) flips `HasOverride` back to false at
        // frame start; the next apply re-writes it from the current
        // keyframe. This is what makes the dirty cascade fire every
        // frame even on hold-span keyframes — the regular setter's
        // `m_X == value` early-out compares against the static field,
        // which doesn't move during animation.
        //
        // Per-property animation override storage was removed in favor
        // of cloning artboard instances (each `ArtboardInstance` lives
        // in its own SubArena with deep-cloned Cores). Edits land in
        // `m_main`; the property hook propagates to clones; animation
        // playback writes directly to the playing clone's `m_X`; reset
        // re-applies m_main values to the clone via the same hook
        // path. See [[clone-only-playback-state]] in the project memory.
        // No per-Core override fields emitted.
        if (property.isSidecar) {
          addPreprocessorEnd(fieldBuf);
        }
        endPropertyGuards(fieldBuf, property);
      }

      // Lazily-allocated sidecar holders: one 8-byte pointer per cluster, null
      // until a property in the cluster is authored. Runtime builds only.
      if (sidecars.isNotEmpty) {
        code.writeln('#ifndef $withRiveEditorPreprocessor');
        for (final entry in sidecars.entries) {
          final structName = _sidecarStructName(entry.value.first);
          code.writeln('Sidecar<$structName> m_${entry.key};');
        }
        code.writeln('#endif');
      }

      // Write getter/setters.
      code.writeln('public:');
      for (final property in properties) {
        final acc = toExt(property) ? extAccessors : code;
        startPropertyGuards(acc, property);
        if (property.isBitmaskPassthrough) {
          // Bitmask-passthrough bool: the persistent storage lives on
          // the target uint (`m_<Target>`). Emit accessors that read /
          // write a single bit of that field, fire the editor hook
          // with bool old/new values (keyed on this property's
          // propertyKey, not the target's), and call the per-property
          // `<name>Changed()` callback. Core registry dispatches into
          // `setBool` / `getBool` for these — the matching real setter
          // must exist or the build breaks; before this branch was
          // populated the dispatch called a method that didn't exist.
          final target = property.bitmaskTargetProperty!;
          final targetField = 'm_${target.capitalizedName}';
          // Notify the BITMASK TARGET's changed callback (e.g.
          // `traitFlagsChanged()`), not the per-bit one. Subclasses
          // override the target callback (e.g. `SemanticData::
          // traitFlagsChanged` propagates the new flags to its
          // SemanticNode + dirties content) — firing the per-bit
          // `<name>Changed()` instead leaves the side-effects
          // unrun and produces silent runtime divergence.
          final changedFn = '${target.cppAccessor}Changed';
          if (property.type.registryType.name == 'uint') {
            // uint slice: read/write `passthroughBitWidth` bits at
            // `passthroughBit` inside the target mask, via the generated
            // `<name>BitOffset` / `<name>FieldMask` constants so adapters
            // share one source of truth. Mirrors master's registry-side
            // read-modify-write, hoisted into a real accessor pair so the
            // editor hook + registry dispatch stay uniform with bools.
            final offsetName = '${property.name}BitOffset';
            final maskName = '${property.name}FieldMask';
            acc.writeln(
                'inline ${property.type.cppGetterName} '
                '${property.cppAccessor}() const { return '
                '($targetField & $maskName) >> $offsetName; }');
            acc.writeln(
                'void ${property.cppAccessor}'
                '(${property.type.cppName} value) {');
            acc.writeln('const ${property.type.cppName} prev = '
                '($targetField & $maskName) >> $offsetName;');
            acc.writeln('if (prev == value) { return; }');
            acc.writeln(
                'RIVE_EDITOR_CHANGING(${property.name}PropertyKey, &prev, '
                '&value);');
            acc.writeln('$targetField = ($targetField & ~$maskName) | '
                '((value << $offsetName) & $maskName);');
          } else {
            final maskName = '${property.name}Bitmask';
            acc.writeln(
                'inline bool ${property.cppAccessor}() const { return '
                '($targetField & $maskName) != 0; }');
            acc.writeln('void ${property.cppAccessor}(bool value) {');
            acc.writeln(
                'const bool prev = ($targetField & $maskName) != 0;');
            acc.writeln('if (prev == value) { return; }');
            acc.writeln(
                'RIVE_EDITOR_CHANGING(${property.name}PropertyKey, &prev, '
                '&value);');
            acc.writeln(
                '$targetField = value ? ($targetField | $maskName) : '
                '($targetField & ~$maskName);');
          }
          acc.writeln('RIVE_EDITOR_CHANGED($changedFn());');
          // Push-notify on the MASK property's key — matches master's
          // path, which routes registry writes through the mask's own
          // setter (mask Changed + notify(mask key)).
          acc.writeln('notifyPropertyChanged(${target.name}PropertyKey);');
          acc.writeln('}');
          endPropertyGuards(acc, property);
          continue;
        }
        // A stored mask property (e.g. colorValue) that satisfies a mixin's
        // host-provided pure virtual must be marked `override`.
        final overridesMixinMask = _mixinsOf.any((m) => m.properties.any((mp) =>
            mp.bitmaskTargetIsHostProvided &&
            mp.passthroughForBitmask == property.name));
        if (!property.getExportType().storesData) {
          acc.writeln((property.isSetOverride ? '' : 'virtual ') +
              'void ${property.cppAccessor}' +
              '(const ${property.type.cppName}& value) ' +
              (property.isSetOverride ? 'override' : '') +
              '= 0;');
        } else if (property.isEncoded) {
          // Encoded properties just have a pure virtual decoder that needs to
          // be implemented. Also requires an implemention of copyPropertyName
          // as that will no longer automatically be copied by the generated
          // acc.
          acc.writeln((property.isSetOverride ? '' : 'virtual ') +
              'void decode${property.capitalizedName}' +
              '(${property.type.cppName} value) ' +
              (property.isSetOverride ? 'override' : '') +
              '= 0;');
          acc.writeln((property.isSetOverride ? '' : 'virtual ') +
              'void copy${property.capitalizedName}' +
              '(const ${_name}Base& object) ' +
              (property.isSetOverride ? 'override' : '') +
              '= 0;');
        } else if (property.isPassthrough) {
          acc.writeln('virtual void '
              'set${property.capitalizedName}(${property.type.cppGetterName} value) '
              '= 0;');
          acc.writeln(
              'virtual ${property.type.cppGetterName} ${property.cppAccessor}() '
              '= 0;');
          acc.writeln(
              'void ${property.cppAccessor}(${property.type.cppName} value) ' +
                  (property.isSetOverride ? 'override' : '') +
                  '{'
                      'if(${property.cppAccessor}() == value)'
                      '{return;}'
                      'set${property.capitalizedName}(value);'
                      '${property.cppAccessor}Changed();'
                      'notifyPropertyChanged(${property.name}PropertyKey);'
                      '}');
        } else if (property.isSidecar) {
          // Editor builds store the full member and run the change hooks
          // (mirrors the plain stored-property emission, minus the runtime
          // no-editor fallback — we're already inside the editor branch).
          addPreprocessorStart(acc, withRiveEditorPreprocessor);
          acc.writeln('inline ${property.type.cppGetterName} '
              '${property.cppAccessor}() const '
              '{ return m_${property.capitalizedName}; }');
          acc.writeln(
              'void ${property.cppAccessor}(${property.type.cppName} value) {');
          acc.writeln('if(m_${property.capitalizedName} == value){return;}');
          if (property.type.name == 'String') {
            acc.writeln('RIVE_EDITOR_STRING_CHANGING('
                '${property.name}PropertyKey,'
                'm_${property.capitalizedName},value);');
          } else if (property.type.name == 'FractionalIndex') {
            acc.writeln(
                'RIVE_EDITOR_FRACTIONAL_INDEX_CHANGING('
                '${property.name}PropertyKey,'
                'm_${property.capitalizedName},value);');
          } else {
            acc.writeln('RIVE_EDITOR_CHANGING(${property.name}PropertyKey,'
                '&m_${property.capitalizedName},&value);');
          }
          acc.writeln('m_${property.capitalizedName} = value;');
          acc.writeln(
              'RIVE_EDITOR_CHANGED(${property.cppAccessor}Changed());');
          acc.writeln('notifyPropertyChanged(${property.name}PropertyKey);');
          acc.writeln('}');
          acc.writeln('#else');
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
            acc.writeln('inline $getterType ${property.name}() const {'
                'static const ${property.type.cppName} defaultValue = '
                '$defaultLiteral;'
                'auto* sidecar = $memberName.get();'
                'return sidecar != nullptr ? sidecar->${property.name} : '
                'defaultValue; }');
          } else {
            final defaultLiteral = converted ?? '$getterType{}';
            acc.writeln('inline $getterType '
                '${property.name}() const { auto* sidecar = $memberName.get(); '
                'return sidecar != nullptr ? sidecar->${property.name} : '
                '$defaultLiteral; }');
          }
          acc.writeln('void ${property.name}(${property.type.cppName} value) {'
              'if(${property.name}() == value){return;}'
              '$memberName.ensureAllocated()->${property.name} = value;'
              '${property.name}Changed();'
              'notifyPropertyChanged(${property.name}PropertyKey);'
              '}');
          addPreprocessorEnd(acc);
        } else {
          // Editor-mode getter composes the static field with the
          // animation-overlay field (`m_XHasOverride ? m_XOverride :
          // Plain field read for both builds — animation playback now
          // writes directly to `m_X` on the playing clone, so there's
          // no override slot to compose. See [[clone-only-playback-state]].
          acc.writeln(((property.isVirtual || property.isPureVirtual)
                  ? 'virtual'
                  : 'inline') +
              ' ${property.type.cppGetterName} ' +
              '${property.cppAccessor}() const ' +
              ((property.isGetOverride || overridesMixinMask)
                  ? 'override'
                  : '') +
              '{ return m_${property.capitalizedName}; }');
          if (!property.isPureVirtual) {
            // Editor-mode hook: every concrete setter notifies
            // Core::onPropertyChanging before mutating so editor_native can
            // record the (old, new) pair into the ChangeTracker (coop + undo
            // journal). Emitted across separate writelns so clang-format
            // keeps the preprocessor directives on their own lines.
            //
            // Type dispatch: String-typed properties use the typed
            // `onStringChanging(key, const std::string&, const std::string&)`
            // hook. The generic void* hook can't safely extract bytes
            // from std::string (SSO layout) vs Span<const uint8_t> given
            // only CoreStringType::id == CoreBytesType::id == 1 — the
            // shared id is the file-format ToC's 2-bit skip class, not a
            // C++-storage discriminator. Typed hook resolves it at the
            // call site. All other types (double, uint, color, bool) go
            // through the void-pointer path.
            // Animation-driven writes go through the same setter
            // path as edit-driven writes — the routing lives in the
            // hook (`handlePropertyChanging` in editor_native's
            // core_hook.cpp), which checks
            // `Core::isAnimationContextActive()` and records a touch
            // for the next frame's `revertTouchedClonesToMain` pass
            // instead of journaling / coop-broadcasting / propagating
            // to other clones. The setter stays generator-uniform.
            // See [[clone-only-playback-state]].
            final isStringField = property.type.name == 'String';
            final isFractionalIndexField =
                property.type.name == 'FractionalIndex';
            acc.writeln(
                'void ${property.cppAccessor}(${property.type.cppName} value) ' +
                    ((property.isSetOverride || overridesMixinMask)
                        ? 'override'
                        : '') +
                    '{');
            acc.writeln(
                'if(m_${property.capitalizedName} == value){return;}');
            if (isStringField) {
              acc.writeln('RIVE_EDITOR_STRING_CHANGING('
                  '${property.name}PropertyKey,'
                  'm_${property.capitalizedName},value);');
            } else if (isFractionalIndexField) {
              // FractionalIndex is a two-field struct — the generic
              // void-pointer hook can't safely carry both halves
              // across a single `void*`. The typed hook captures both
              // values by value at the call site.
              acc.writeln(
                  'RIVE_EDITOR_FRACTIONAL_INDEX_CHANGING('
                  '${property.name}PropertyKey,'
                  'm_${property.capitalizedName},value);');
            } else {
              acc.writeln('RIVE_EDITOR_CHANGING(${property.name}PropertyKey,'
                  '&m_${property.capitalizedName},&value);');
            }
            acc.writeln('m_${property.capitalizedName} = value;');
            // The `${name}Changed()` callback is where runtime side
            // effects live (markDirty, parent-chain walks, resolve
            // calls, etc.) — they assume the object is fully wired up
            // (`m_Artboard` / `m_Parent` set). In editor mode, property
            // mutations can fire during coop-apply hydration BEFORE
            // `onAddedDirty` has wired those pointers, so we gate the
            // callback on `hasValidated()`. editor_native flips that
            // flag post-`onAddedClean`; mirrors Dart's `hasValidated`
            // guard in packages/rive_core/**/*_base.dart setters.
            // Runtime builds keep the unconditional call so there's
            // zero overhead and nothing to flip.
            acc.writeln(
                'RIVE_EDITOR_CHANGED(${property.cppAccessor}Changed());');
            // Push-notification for target->source data binds — mirrors
            // master's plain setter. Cheap no-observer null check; editor
            // arena Cores never subscribe (push stays disabled there), so
            // this only fires for pump-runtime instances.
            acc.writeln(
                'notifyPropertyChanged(${property.name}PropertyKey);');
            acc.writeln('}');
            // Standalone `xAnimate` / `xResetOverride` emission was
            // removed with the move to clone-only playback. The
            // regular setter's animation-context branch above
            // handles playback writes; reset-to-base is now done via
            // `EditorFile::revertTouchedClonesToMain()` which
            // re-applies m_main values to clones through the
            // clone-sync hook path. See [[clone-only-playback-state]].
          } else {
            acc.writeln(
                '''virtual void ${property.cppAccessor}(${property.type.cppName} value) = 0;''');
          }
        }
        endPropertyGuards(acc, property);

        acc.writeln();
      }
    }

    if (!_isAbstract) {
      code.writeln('Core* clone() const override;');
    }

    if (storedPropertiesNoPassthrough.isNotEmpty || _extensionOf == null) {
      code.writeln('void copy(const ${_name}Base& object) {');
      for (final property in storedPropertiesNoPassthrough) {
        final copyBuf = toExt(property) ? extCopy : code;
        startPropertyGuards(copyBuf, property);
        if (property.isSidecar) {
          // Full member exists only in editor builds; runtime copies the
          // cluster holder once below.
          addPreprocessorStart(copyBuf, withRiveEditorPreprocessor);
        }
        if (property.isEncoded) {
          copyBuf.writeln('copy${property.capitalizedName}(object);');
        } else {
          copyBuf.writeln('m_${property.capitalizedName} = '
              'object.m_${property.capitalizedName};');
        }
        if (property.isSidecar) {
          addPreprocessorEnd(copyBuf);
        }
        endPropertyGuards(copyBuf, property);
      }
      // Deep-copy each sidecar cluster (Sidecar<T> handles null vs allocated).
      if (sidecars.isNotEmpty) {
        code.writeln('#ifndef $withRiveEditorPreprocessor');
        for (final entry in sidecars.entries) {
          code.writeln('m_${entry.key} = object.m_${entry.key};');
        }
        code.writeln('#endif');
      }
      if (extCopy.isNotEmpty) {
        code.writeln('RIVE_EDITOR_COPY(object);');
      }
      if (_extensionOf != null) {
        code.writeln('${_extensionOf!.name}::'
            'copy(object); ');
      } else {
        // Root of the copy chain: carry the editor validation flag.
        // clone() copies fields, never the Core base, so without this
        // every instance()/createInstance clone comes out unvalidated
        // and the hasValidated() gate starves its Changed() callbacks.
        code.writeln('RIVE_EDITOR_COPY_VALIDATED(object);');
      }
      code.writeln('}');
      code.writeln();

      code.writeln('bool deserialize(uint16_t propertyKey, '
          'BinaryReader& reader) override {');

      final runtimeStored =
          storedPropertiesNoPassthrough.where((p) => !toExt(p));
      if (storedPropertiesNoPassthrough.isNotEmpty) {
        final switchBuf = runtimeStored.isEmpty ? extDeserialize : code;
        if (runtimeStored.isNotEmpty) {
          code.writeln('switch (propertyKey){');
        }
        for (final property in storedPropertiesNoPassthrough) {
          final code = toExt(property) ? extDeserialize : switchBuf;
          startPropertyGuards(code, property);
          code.writeln('case ${property.name}PropertyKey:');
          // `deserialize(propertyKey, reader)` is the `.riv` (runtime
          // file format) read path — a single varuint for Id-typed
          // fields. `runtimeDeserializeFunction` picks that variant
          // for `IdFieldType` and matches `deserializeFunction` for
          // every other field type.
          if (property.isSidecar) {
            // Editor builds read into the full member; runtime builds
            // into the lazily-allocated cluster.
            addPreprocessorStart(code, withRiveEditorPreprocessor);
            code.writeln('m_${property.capitalizedName} = '
                '${property.type.runtimeDeserializeFunction}(reader);');
            code.writeln('#else');
            code.writeln('m_${property.sidecarName}.ensureAllocated()'
                '->${property.name} '
                '= ${property.type.runtimeDeserializeFunction}(reader);');
            addPreprocessorEnd(code);
          } else if (property.isEncoded) {
            code.writeln('decode${property.capitalizedName}'
                '(${property.type.runtimeDeserializeFunction}(reader));');
          } else {
            code.writeln('m_${property.capitalizedName} = '
                '${property.type.runtimeDeserializeFunction}(reader);');
          }
          code.writeln('return true;');
          endPropertyGuards(code, property);
        }
        if (runtimeStored.isNotEmpty) {
          code.writeln('}');
        }
      }
      if (extDeserialize.isNotEmpty) {
        code.writeln('RIVE_EDITOR_DESERIALIZE(propertyKey, reader);');
      }
      if (_extensionOf != null) {
        code.writeln('return ${_extensionOf!.name}::'
            'deserialize(propertyKey, reader); }');
      } else {
        code.writeln('return false; }');
      }

      // Parallel applyChange(propertyKey, reader) that routes through the
      // public setter so xChanged() + the change hooks fire. It lives in the
      // editor extension, so the shipped runtime never sees it. Setter calls
      // are qualified with `this->` so they unambiguously name the member
      // function, not the parameter (which would collide for properties
      // literally named `propertyKey`, `key`, etc. — e.g.
      // KeyedProperty.propertyKey, DataEnumValue.key).
      //
      // `deserializeFunction` is the `.rev` (editor coop) read path — two
      // varuints for Id-typed fields. Every other field type shares the
      // runtime read path.
      final editorBody = splitEditorExt ? extMethods : code;
      editorBody.writeln('bool applyChange(uint16_t propertyKey, '
          'BinaryReader& reader) override {');
      // Build the applyChange switch from the union of normal stored
      // properties + bitmask-passthrough bools. The latter aren't in
      // `storedPropertiesNoPassthrough` (their storage lives on the
      // target uint), but they DO need an applyChange entry so
      // `replayJournalEntry` can route the bool key to the bit-setter
      // emitted above.
      final applyChangeProperties = properties.where((property) =>
          property.getExportType().storesData && !property.isPassthrough);
      if (applyChangeProperties.isNotEmpty) {
        editorBody.writeln('switch (propertyKey){');
        for (final property in applyChangeProperties) {
          if (property.isWithRiveToolsOnly) {
            addPreprocessorStart(editorBody, withRiveToolsPreprocessor);
          }
          editorBody.writeln('case ${property.name}PropertyKey:');
          if (property.isEncoded) {
            editorBody.writeln('this->decode${property.capitalizedName}'
                '(${property.type.deserializeFunction}(reader));');
          } else {
            // this->${name}(value) → public setter on this class. Sidecar
            // properties route here too: applyChange is editor-only, where
            // the setter writes the full member.
            editorBody.writeln('this->${property.cppAccessor}'
                '(${property.type.deserializeFunction}(reader));');
          }
          editorBody.writeln('return true;');
          if (property.isWithRiveToolsOnly) {
            addPreprocessorEnd(editorBody);
          }
        }
        editorBody.writeln('}');
      }
      if (_extensionOf != null) {
        editorBody.writeln('return ${_extensionOf!.name}::'
            'applyChange(propertyKey, reader); }');
      } else {
        // Top-level types still bottom out at `Core::applyChange` so
        // any future Core-level keys have a dispatch target. Today the
        // virtual just returns false — every editor-only property is a
        // generated field on its owning *Base class.
        editorBody.writeln(
            'return Core::applyChange(propertyKey, reader); }');
      }

      // Editor-mode: captureStateForJournal() — emits an identity
      // PropertyChange through the hook for every stored, non-default
      // property. Used by handleDeleteEditorObject to snapshot the
      // object's state into the active JournalBatch before deletion, so
      // undo of the delete restores the full pre-delete state via the
      // normal applyChange path.
      //
      // Mirrors packages/core_generator/lib/src/definition.dart's
      // `changeNonDefault` emission (Dart editor's rive_core generator).
      // Properties at their initial value are skipped to keep journal
      // entries minimal. Passthrough + encoded properties are excluded
      // (no stored backing field to compare against).
      //
      // String + FractionalIndex use their typed hooks
      // (`onStringChanging`, `onFractionalIndexChanging`); everything
      // else uses the generic `onPropertyChanging` with a pointer
      // pair — same dispatch as the generated setters.
      editorBody.writeln('void captureStateForJournal() override {');
      if (_extensionOf != null) {
        editorBody.writeln(
            '${_extensionOf!.name}::captureStateForJournal();');
      } else {
        // Top-level types still chain to `Core::captureStateForJournal`
        // so any future Core-level editor state has a dispatch target.
        // Today the base implementation is a no-op.
        editorBody.writeln('Core::captureStateForJournal();');
      }
      for (final property in storedPropertiesNoPassthrough) {
        if (property.isEncoded) {
          // Encoded properties have no stored field — skip.
          continue;
        }
        if (property.isWithRiveToolsOnly) {
          addPreprocessorStart(editorBody, withRiveToolsPreprocessor);
        }
        final initialize = property.initialValueRuntime ??
            property.initialValue ??
            property.type.defaultValue;
        if (initialize != null) {
          final converted = property.type.convertCpp(initialize);
          if (converted != null) {
            final field = 'm_${property.capitalizedName}';
            final isString = property.type.name == 'String';
            final isFractionalIndex =
                property.type.name == 'FractionalIndex';
            editorBody.writeln('if ($field != $converted) {');
            if (isString) {
              editorBody.writeln(
                  'onStringChanging(${property.name}PropertyKey, '
                  '$field, $field);');
            } else if (isFractionalIndex) {
              editorBody.writeln(
                  'onFractionalIndexChanging(${property.name}PropertyKey, '
                  '$field, $field);');
            } else {
              editorBody.writeln(
                  'onPropertyChanging(${property.name}PropertyKey, '
                  '&$field, &$field);');
            }
            editorBody.writeln('}');
          }
        }
        if (property.isWithRiveToolsOnly) {
          addPreprocessorEnd(editorBody);
        }
      }
      editorBody.writeln('}');
    }

    if (storedProperties.any((prop) => !toExt(prop))) {
      code.writeln('protected:');
    }
    if (storedProperties.isNotEmpty) {
      for (final property in storedProperties) {
        // A bitmask passthrough deliberately fires the mask's callback,
        // never its own — emitting one is an override that can never run.
        if (property.isBitmaskPassthrough) {
          continue;
        }
        final changedBuf = toExt(property) ? extChanged : code;
        startPropertyGuards(changedBuf, property);
        // If a parent class already declares a property with this
        // accessor name (different key, shadowing), the emitted
        // `<name>Changed()` overrides the parent's virtual — clang
        // (`-Winconsistent-missing-override`) warns without `override`.
        // Walks the runtime extension chain to detect it. Concrete
        // example: `LibraryArtboard.name` (key 794) shadows
        // `Asset.name` (key 203); both classes get a `nameChanged()`
        // virtual and the child must mark `override`.
        bool overrides = false;
        for (var ancestor = _extensionOf;
            ancestor != null;
            ancestor = ancestor._extensionOf) {
          for (final ap in ancestor.storedProperties) {
            if (ap.isBitmaskPassthrough) {
              continue;
            }
            if (ap.cppAccessor == property.cppAccessor) {
              overrides = true;
              break;
            }
          }
          if (overrides) break;
        }
        if (overrides) {
          changedBuf
              .writeln('void ${property.cppAccessor}Changed() override {}');
        } else {
          changedBuf
              .writeln('virtual void ${property.cppAccessor}Changed() {}');
        }
        endPropertyGuards(changedBuf, property);
      }
    }
    if (hasEditorExt) {
      addPreprocessorStart(code, withRiveEditorPreprocessor);
      code.writeln('#include "$editorExtIncludeFilename"');
      addPreprocessorEnd(code);
    }
    code.writeln('};');
    code.writeln('}');
    if (editorOnly) {
      addPreprocessorEnd(code);
    }

    var file = File('$_outputGeneratedHppPath$localCodeFilename');
    file.createSync(recursive: true);

    var formattedCode =
        await _formatter.formatAndGuard('${_name}Base', code.toString());
    file.writeAsStringSync(formattedCode, flush: true);

    if (hasEditorExt) {
      await _writeEditorExtension(
        keys: extKeys,
        fields: extFields,
        accessors: extAccessors,
        copy: extCopy,
        deserialize: extDeserialize,
        methods: extMethods,
        changed: extChanged,
      );
    }

    // See if we need to stub out the concrete version... Editor-only
    // types (`"runtime": false`) are wrapped in the same
    // `#ifdef WITH_RIVE_EDITOR` so the runtime build never sees the
    // class declaration.
    var concreteFile = File('$_outputConcreteHppPath$concreteCodeFilename');
    if (!concreteFile.existsSync()) {
      StringBuffer concreteCode = StringBuffer();
      concreteFile.createSync(recursive: true);
      concreteCode.writeln('#include "$generatedIncludeFilename"');
      concreteCode.writeln('#include <stdio.h>');
      if (editorOnly) {
        addPreprocessorStart(concreteCode, withRiveEditorPreprocessor);
      }
      concreteCode.writeln('namespace rive {');
      concreteCode.writeln('''class $_name : public ${_name}Base {
        public:
      };''');
      concreteCode.writeln('}');
      if (editorOnly) {
        addPreprocessorEnd(concreteCode);
      }

      var formattedCode =
          await _formatter.formatAndGuard(_name!, concreteCode.toString());
      concreteFile.writeAsStringSync(formattedCode, flush: true);
    }
    if (!_isAbstract) {
      StringBuffer cppCode = StringBuffer();
      cppCode.writeln('#include "$generatedIncludeFilename"');
      cppCode.writeln('#include "$concreteCodeFilename"');
      cppCode.writeln();
      if (editorOnly) {
        addPreprocessorStart(cppCode, withRiveEditorPreprocessor);
      }
      cppCode.writeln('using namespace rive;');
      cppCode.writeln();
      cppCode.writeln('Core* ${_name}Base::clone() const { '
          'auto cloned = new $_name(); '
          'cloned->copy(*this); '
          'return cloned; '
          '}');
      if (editorOnly) {
        addPreprocessorEnd(cppCode);
      }
      var cppFile = File('$_outputGeneratedCppPath$localCppCodeFilename');
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
    // Editor-only mixins never reach C++ (see addMixin), so drop them
    // before anything iterates the def tree.
    definitions.removeWhere(
        (key, definition) => definition._isMixin && !definition._forRuntime);

    // Clear out previous generated code — both runtime and editor
    // trees, headers and cpp — so stale bases from deleted defs (or
    // defs whose `runtime` flag flipped from true → false / vice versa)
    // don't linger. Concrete subclasses (hand-edited, written under
    // `_outputConcreteHppPath`) are NOT cleared here; they're emitted
    // once when missing and persist across regens.
    for (final path in [
      generatedHppPath,
      editorGeneratedHppPath,
      generatedCppPath,
      editorGeneratedCppPath,
    ]) {
      var dir = Directory(path);
      if (dir.existsSync()) {
        dir.deleteSync(recursive: true);
      }
      dir.createSync(recursive: true);
    }
    // Also clear the editor-only concrete stub tree so moved-out
    // types don't leave orphans behind in the runtime include dir.
    // (Runtime concrete stubs are hand-edited elsewhere; the
    // generator only creates them once via
    // `!concreteFile.existsSync()` and never overwrites them.)
    // Generate core context.

    for (final definition in definitions.values) {
      await definition.generateCode();
    }

    await _writeEditorFieldTypes();
    await _writeRegistry(forEditor: false);
    await _writeRegistry(forEditor: true);

    return true;
  }

  /// Field-type headers the class-body extensions need. They can't carry
  /// their own includes, so every runtime base with an extension pulls this
  /// one header in behind its editor guard.
  static Future<void> _writeEditorFieldTypes() async {
    var includes = <String>{};
    for (final definition in definitions.values) {
      if (!definition.forRuntime) {
        continue;
      }
      for (final property in definition.properties) {
        if (!property.isWithRiveEditorOnly) {
          continue;
        }
        var include = property.type.include;
        if (include != null) {
          includes.add(include);
        }
        includes.add('rive/core/field_types/' +
            property.type.snakeRuntimeCoreName +
            '.hpp');
      }
    }
    StringBuffer code = StringBuffer();
    for (final include in includes.toList()..sort()) {
      if (include[0] == '<') {
        code.writeln('#include $include');
      } else {
        code.writeln('#include "$include"');
      }
    }
    var file = File('${editorGeneratedHppPath}editor_field_types.hpp');
    file.createSync(recursive: true);
    file.writeAsStringSync(
        await _formatter.formatAndGuard('EditorFieldTypes', code.toString()),
        flush: true);
  }

  /// Emits a by-key dispatch table over the def tree. The runtime table
  /// (`CoreRegistry`, `rive/generated`) sees only runtime types and
  /// properties; the editor table (`EditorCoreRegistry`, shipped with the
  /// kernel) is the superset the editor dispatches against.
  static Future<void> _writeRegistry({required bool forEditor}) async {
    final defs = definitions.values
        .where((definition) => forEditor || definition.forRuntime)
        .toList();

    // A property is editor-only either because its class is `"runtime":
    // false` or because the property itself is.
    bool propertyIsEditorOnly(Property property) =>
        !property.definition.forRuntime || property.isWithRiveEditorOnly;
    bool includeProperty(Property property) =>
        forEditor ||
        (!propertyIsEditorOnly(property) &&
            !property.type.registryType.isWithRiveEditorOnly);

    StringBuffer ctxCode = StringBuffer('');
    var includes = <String>{};
    for (final definition in defs) {
      // Mixins have no concrete class; include their (constants-only) base
      // header directly so the registry can reference the shared key
      // constants.
      includes.add(definition._isMixin
          ? 'rive/generated/${definition.localCodeFilename}'
          : definition.concreteCodeFilename);
    }
    for (final include in includes.toList()..sort()) {
      ctxCode.writeln('#include "$include"');
    }

    final className = forEditor ? 'EditorCoreRegistry' : 'CoreRegistry';
    ctxCode.writeln('namespace rive {class $className {'
        'public:');
    ctxCode.writeln('static Core* makeCoreInstance(int typeKey) {'
        'switch(typeKey) {');
    for (final definition in defs) {
      if (definition._isAbstract) {
        continue;
      }
      ctxCode.writeln('case ${definition.name}Base::typeKey:');
      ctxCode.writeln('return new ${definition.name}();');
    }
    ctxCode.writeln('} return nullptr; }');

    if (forEditor) {
      // Human-readable class name for a typeKey, for the editor's coverage
      // and debug dumps. Abstract types are listed too: a coop ChangeSet
      // naming one is a bug worth surfacing by name.
      ctxCode.writeln('static const char* typeName(int typeKey) {'
          'switch(typeKey) {');
      for (final definition in defs) {
        // Mixin interface bases have key constants but no typeKey member.
        if (definition._key?.intValue == null || definition._isMixin) {
          continue;
        }
        ctxCode.writeln('case ${definition.name}Base::typeKey:');
        ctxCode.writeln('return "${definition.name}";');
      }
      ctxCode.writeln('} return "(unknown)"; }');
    }

    var usedFieldTypes = <FieldType, List<Property>>{};
    var getSetFieldTypes = <FieldType, List<Property>>{};
    for (final definition in defs) {
      for (final property in definition.properties) {
        if (!includeProperty(property)) {
          continue;
        }
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

    // Runtime callers (`KeyFrameId::apply`, `KeyFrameUint::apply`,
    // `data_bind/context/context_value_*`, …) reach into
    // `CoreRegistry::setUint` even for parentId / targetId, because `Id`
    // aliases `uint32_t` in runtime-only builds. Without the duplicate cases
    // the dispatch falls through and the setter never fires. The editor
    // routes Id properties through set/getId instead, so its table skips
    // them.
    final idProperties = <Property>[];
    for (final fieldType in getSetFieldTypes.keys) {
      if (fieldType.name == 'Id') {
        idProperties.addAll(getSetFieldTypes[fieldType]!);
      }
    }
    void emitIdCasesForUint(StringBuffer code, bool isGetter) {
      if (forEditor || idProperties.isEmpty) {
        return;
      }
      for (final property in idProperties) {
        if (property.isWithRiveToolsOnly) {
          addPreprocessorStart(code, withRiveToolsPreprocessor);
        }
        code.writeln('case ${property.definition.name}Base'
            '::${property.name}PropertyKey:');
        if (property.key != null) {
          for (final altKey in property.key!.alternates) {
            code.writeln('case ${property.definition.name}Base'
                '::${altKey.stringValue}PropertyKey:');
          }
        }
        if (isGetter) {
          code.writeln('return object->as<${property.definition.name}Base>()->'
              '${property.cppAccessor}();');
        } else {
          code.writeln('object->as<${property.definition.name}Base>()->'
              '${property.cppAccessor}(value);');
          code.writeln('break;');
        }
        if (property.isWithRiveToolsOnly) {
          addPreprocessorEnd(code);
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
          } else {
            // Same-def bitmask passthroughs route through their generated
            // bit accessor, which does the masked read-modify-write and
            // fires the editor hooks.
            ctxCode.writeln('case ${property.definition.name}Base'
                '::${property.name}PropertyKey:');
            if (property.key != null) {
              for (final altKey in property.key!.alternates) {
                ctxCode.writeln('case ${property.definition.name}Base'
                    '::${altKey.stringValue}PropertyKey:');
              }
            }
            ctxCode.writeln('object->as<${property.definition.name}Base>()->'
                '${property.cppAccessor}(value);');
            ctxCode.writeln('break;');
          }
          if (property.isWithRiveToolsOnly) {
            addPreprocessorEnd(ctxCode);
          }
        }
      }
      if (fieldType.name == 'uint') {
        emitIdCasesForUint(ctxCode, false);
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
          } else {
            ctxCode.writeln(
                'return object->as<${property.definition.name}Base>()->'
                '${property.cppAccessor}();');
          }
          if (property.isWithRiveToolsOnly) {
            addPreprocessorEnd(ctxCode);
          }
        }
      }
      if (fieldType.name == 'uint') {
        emitIdCasesForUint(ctxCode, true);
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

    // Uint properties hold between keyframes by default (they're mostly enums
    // and ids). The ones flagged `interpolates` in their def opt into being
    // tweened instead; KeyFrameUint consults this to decide.
    ctxCode.writeln('''
      static bool isInterpolatableUint(uint32_t propertyKey) {
        switch(propertyKey) {''');
    for (final fieldType in usedFieldTypes.keys) {
      var properties = usedFieldTypes[fieldType];
      if (properties != null) {
        bool found = false;
        for (final property in properties) {
          if (property.interpolates &&
              property.getExportType().registryType.name == 'uint') {
            found = true;
            ctxCode.writeln('case ${property.definition._name}Base'
                '::${property.name}PropertyKey:');
          }
        }
        if (found) {
          ctxCode.writeln('return true;');
        }
      }
    }
    ctxCode.writeln('default:return false;');
    ctxCode.writeln('}}');

    // CoreIntType shares CoreUintType's field id (on the wire it is a varuint),
    // so propertyFieldId can't tell the two apart. Anything switching on the
    // field id and then writing a value needs this to pick set/getInt. Keyed
    // off the same group that generates them, so the three stay in step.
    ctxCode.writeln('''
      static bool isSignedInt(uint32_t propertyKey) {
        switch(propertyKey) {''');
    final intProperties = getSetFieldTypes[FieldType.find('int')];
    if (intProperties != null && intProperties.isNotEmpty) {
      for (final property in intProperties) {
        if (property.isWithRiveToolsOnly) {
          addPreprocessorStart(ctxCode, withRiveToolsPreprocessor);
        }
        ctxCode.writeln('case ${property.definition._name}Base'
            '::${property.name}PropertyKey:');
        if (property.isWithRiveToolsOnly) {
          addPreprocessorEnd(ctxCode);
        }
      }
      ctxCode.writeln('return true;');
    }
    ctxCode.writeln('default:return false;');
    ctxCode.writeln('}}');

    if (forEditor) {
      // editorFieldType: editor-scoped property-type discriminator.
      //
      // `propertyFieldId` above returns the runtime TOC size class
      // (CoreUintType::id, CoreIdType::id, CoreFractionalIndexType::id all =
      // 0; CoreStringType::id and CoreBytesType::id both = 1). That collision
      // is correct for the `.riv` wire format — one varuint decode path for
      // all class-0 types — but not enough for editor dispatch, which needs to
      // know whether `parentId` is an Id (two varuints on the editor wire) vs
      // a Uint, whether `childOrder` is a FractionalIndex vs a Uint, and so
      // on.
      //
      // Tags are drawn from the same `usedFieldTypes` map so the case-label
      // ordering matches `propertyFieldId` exactly.
      const editorFieldTypeTags = <String, int>{
        'uint': 0,
        'String': 1,
        'double': 2,
        'Color': 3,
        'bool': 4,
        'Id': 5,
        'FractionalIndex': 6,
        'Bytes': 7,
      };
      ctxCode.writeln('static int editorFieldType(int propertyKey) {');
      ctxCode.writeln('switch(propertyKey) {');
      for (final fieldType in usedFieldTypes.keys) {
        if (!fieldType.storesData) continue;
        final tag = editorFieldTypeTags[fieldType.name];
        if (tag == null) continue;
        final properties = usedFieldTypes[fieldType];
        if (properties != null) {
          for (final property in properties) {
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
        ctxCode.writeln('return $tag;');
      }
      ctxCode.writeln('default: return 255;}}');

      // The one real Id discriminator. `propertyFieldId` returns the ToC
      // skip class, where CoreIdType::id == CoreUintType::id == 0, so
      // every uint key looks like an Id to it.
      ctxCode.writeln('''
        static bool isIdProperty(int propertyKey) {
          return editorFieldType(propertyKey) == ${editorFieldTypeTags['Id']};
        }''');

      // Storage width for the varuint size class. The editor's property
      // hook hands the encoder a pointer to the field itself, so a
      // uint8 / uint16 / int16 property read as uint32 runs off the end
      // of its storage.
      ctxCode.writeln('static int propertyStorageBytes(int propertyKey) {');
      ctxCode.writeln('switch(propertyKey) {');
      final byWidth = <int, List<Property>>{};
      for (final fieldType in usedFieldTypes.keys) {
        for (final property in usedFieldTypes[fieldType]!) {
          final width = property.type.storageBytes;
          if (width == null || width == 4) continue;
          (byWidth[width] ??= []).add(property);
        }
      }
      for (final width in byWidth.keys.toList()..sort()) {
        for (final property in byWidth[width]!) {
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
        ctxCode.writeln('return $width;');
      }
      ctxCode.writeln('default: return 4;}}');
    }

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

    StringBuffer body = StringBuffer();
    if (forEditor) {
      addPreprocessorStart(body, withRiveEditorPreprocessor);
      body.write(ctxCode);
      addPreprocessorEnd(body);
    } else {
      body.write(ctxCode);
    }

    var file = File(forEditor
        ? '${editorGeneratedHppPath}editor_core_registry.hpp'
        : '${generatedHppPath}core_registry.hpp');
    file.createSync(recursive: true);
    file.writeAsStringSync(
        await _formatter.formatAndGuard(className, body.toString()),
        flush: true);
  }
}

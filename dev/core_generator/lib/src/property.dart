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

  /// Opt-in for property types whose keyframes hold by default (uint) to
  /// interpolate between keys instead. See the editor-side generator for the
  /// full rationale.
  bool interpolates = false;
  String? group;
  Key? key;
  String? description;
  bool isNullable = false;
  bool isRuntime = true;
  bool isCoop = true;
  bool isWithRiveToolsOnly = false;
  /// Property is declared `"runtime": false` in the def. Lives only in
  /// editor builds (`WITH_RIVE_EDITOR`). Generator wraps the
  /// property-key constant, field storage, getter/setter, and
  /// deserialize/applyChange switch cases in `#ifdef WITH_RIVE_EDITOR`
  /// so runtime builds never see them.
  bool isWithRiveEditorOnly = false;
  bool isSetOverride = false;
  bool isGetOverride = false;
  bool isEncoded = false;
  bool isBindable = false;
  bool isPassthrough = false;
  String? passthroughForBitmask;
  int? passthroughBit;
  int? passthroughBitWidth;
  Property? bitmaskTargetProperty;

  /// True when this is a bitmask passthrough defined in a mixin whose mask
  /// ([passthroughForBitmask]) is provided by the host class that includes the
  /// mixin (e.g. `colorValue` on SolidColor/GradientStop), not a same-def
  /// sibling. core_registry dispatches these per consuming type.
  bool bitmaskTargetIsHostProvided = false;
  bool isPureVirtual = false;
  FieldType? typeRuntime;

  /// Runtime-only: name of the sidecar cluster this property's storage is
  /// hoisted into. All properties in a def sharing a [sidecarName] are packed
  /// into one lazily-allocated struct so the base class pays only an 8-byte
  /// pointer (null) until the cluster is authored. C++ generator only; the Dart
  /// generator ignores the JSON `sidecar` key and keeps the field inline.
  String? sidecarName;
  bool get isSidecar => sidecarName != null;

  bool get isBitmaskPassthrough =>
      passthroughForBitmask != null && passthroughBit != null;

  /// Bit width of a bitmask passthrough field; bools occupy a single bit.
  int get passthroughBitWidthOrDefault => passthroughBitWidth ?? 1;

  static Property? make(
      Definition type, String name, Map<String, dynamic> data) {
    // `"runtime": false` properties are editor-only. Historically the
    // generator dropped them entirely here; now we emit them wrapped in
    // `#ifdef WITH_RIVE_EDITOR` so editor builds can hydrate them via
    // coop (e.g. `ImageAsset.format/quality`, `LayerImageAsset.layer`)
    // while runtime builds compile them out. The `isWithRiveEditorOnly`
    // flag is set in the constructor from `data['runtime']`.

    // Pick the field type. In general we prefer `typeRuntime` (the
    // memory-layout-equivalent runtime type like `uint` for `Enum<…>`)
    // so runtime-only builds don't drag editor-only machinery. For
    // `Id` we want `IdFieldType` in BOTH builds (its C++ storage is
    // `rive::Id`, aliased to `uint32_t` in runtime-only builds and
    // flipped to a `{client, object}` struct under `WITH_RIVE_EDITOR`)
    // — but only when the def explicitly opts the property into the
    // runtime layout via `typeRuntime: "uint"`.
    //
    // Pure editor-only `Id` fields (`"runtime": false`, no
    // `typeRuntime`) — e.g. `KeyedObject.animationId`,
    // `KeyedProperty.keyedObjectId`, `KeyFrame.keyedPropertyId` —
    // have no runtime wire-format mapping and shouldn't appear in
    // `.riv` files, but editor_native still needs to hydrate them
    // over coop (they're how the editor reconstructs the animation
    // graph: child -> parent Id lookup per Dart's onAdded pattern).
    // Emit them using `IdFieldType` under `#ifdef WITH_RIVE_EDITOR`
    // — `isWithRiveEditorOnly` gets set in the constructor by
    // reading `data['runtime']`.
    final explicitType = FieldType.find(data['type']);
    final runtimeOverride = FieldType.find(data['typeRuntime']);
    final isEditorOnly =
        data['runtime'] is bool && data['runtime'] == false;
    // A property on an editor-only class is implicitly editor-only —
    // the whole class is wrapped in `#ifdef WITH_RIVE_EDITOR`, so its
    // `Id` field can use the editor-flavored `IdFieldType` without a
    // `typeRuntime` mapping (no runtime code will ever see it). Without
    // this branch, `ComponentAsset.artboardId` (and similar) silently
    // gets dropped because the per-property `runtime: false` flag isn't
    // set even though the parent class is editor-only.
    final classIsEditorOnly = !type.forRuntime;
    if (explicitType is IdFieldType && runtimeOverride == null &&
        !isEditorOnly && !classIsEditorOnly) {
      // Runtime-visible Id with no `.riv` wire mapping. Dropping it is
      // still the right call, but silently doing so hides a def typo
      // behind a property that simply never appears in the generated
      // class.
      color(
          'Dropping runtime Id property $name on ${type.name}: '
          'needs "typeRuntime": "uint" or "runtime": false.',
          front: Styles.YELLOW);
      return null;
    }
    var fieldType = explicitType is IdFieldType
        ? explicitType
        : (runtimeOverride ?? explicitType);

    if (fieldType == null) {
      // Editor-only properties may use field types that have no C++
      // runtime representation (e.g. `FractionalIndex`, `List<Id>`).
      // These were historically dropped silently by the
      // `"runtime": false` early-return; preserve that behavior now
      // that the early-return is gone so we don't flood the build
      // with false-positive warnings.
      final isEditorOnly =
          data['runtime'] is bool && data['runtime'] == false;
      if (!isEditorOnly) {
        color('Invalid field type ${data['type']} for $name.',
            front: Styles.RED);
      }
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
    dynamic interp = data['interpolates'];
    if (interp is bool) {
      interpolates = interp;
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
    if (r is bool && r == false) {
      // Editor-only property. Leave `isRuntime = true` so it stays in
      // the property list; the generator wraps every emitted artifact
      // in `#ifdef WITH_RIVE_EDITOR`.
      isWithRiveEditorOnly = true;
    }
    dynamic c = data['coop'];
    if (c is bool) {
      isCoop = c;
    }
    dynamic tl = data['withRiveToolsOnly'];
    if (tl is bool) {
      isWithRiveToolsOnly = tl;
    }
    dynamic rt = data['typeRuntime'];
    if (rt is String) {
      typeRuntime = FieldType.find(rt);
    }
    dynamic b = data['bindable'];
    if (b is bool) {
      isBindable = b;
    }
    dynamic p = data['passthrough'];
    if (p is bool) {
      isPassthrough = p;
    }
    dynamic pfb = data['passthroughForBitmask'];
    if (pfb is String) {
      passthroughForBitmask = pfb;
    }
    dynamic pbit = data['passthroughBit'];
    if (pbit is int) {
      passthroughBit = pbit;
    }
    dynamic pbw = data['passthroughBitWidth'];
    if (pbw is int) {
      passthroughBitWidth = pbw;
    }
    dynamic pv = data['pureVirtual'];
    if (pv is bool) {
      isPureVirtual = pv;
    }
    dynamic sc = data['sidecar'];
    if (sc is String && sc.isNotEmpty) {
      sidecarName = sc;
    }
    key = Key.fromJSON(data['key']) ?? Key.forProperty(this);
  }

  FieldType getExportType() => typeRuntime ?? type;

  @override
  String toString() => '$name(${key?.intValue})';

  String get capitalizedName => '${name[0].toUpperCase()}${name.substring(1)}'
      .replaceAll('<', '')
      .replaceAll('>', '');

  /// Capitalized sidecar cluster name, e.g. `cornerRadius` -> `CornerRadius`.
  /// Used to build the struct name and `ensure`/member identifiers.
  String? get capitalizedSidecarName => sidecarName == null
      ? null
      : '${sidecarName![0].toUpperCase()}${sidecarName!.substring(1)}';

  /// C++-safe accessor name. Some property names in the def (e.g.
  /// `export`, `public`, `private`, `class`) collide with C++ keywords.
  /// These were historically invisible because `"runtime": false` made
  /// `make` return null; now that editor-only properties are emitted
  /// under `#ifdef WITH_RIVE_EDITOR`, the accessor/setter/`xChanged`
  /// emission needs a legal identifier. We suffix with `_` — a
  /// conventional, minimal mangling that leaves the wire format
  /// (`propertyKey` int + key string) untouched and is invisible to
  /// anything outside this generator + the emitted class surface.
  static const Set<String> _cppReservedNames = {
    'alignas',
    'alignof',
    'and',
    'and_eq',
    'asm',
    'auto',
    'bitand',
    'bitor',
    'bool',
    'break',
    'case',
    'catch',
    'char',
    'char8_t',
    'char16_t',
    'char32_t',
    'class',
    'compl',
    'concept',
    'const',
    'consteval',
    'constexpr',
    'constinit',
    'const_cast',
    'continue',
    'co_await',
    'co_return',
    'co_yield',
    'decltype',
    'default',
    'delete',
    'do',
    'double',
    'dynamic_cast',
    'else',
    'enum',
    'explicit',
    'export',
    'extern',
    'false',
    'float',
    'for',
    'friend',
    'goto',
    'if',
    'inline',
    'int',
    'long',
    'mutable',
    'namespace',
    'new',
    'noexcept',
    'not',
    'not_eq',
    'nullptr',
    'operator',
    'or',
    'or_eq',
    'private',
    'protected',
    'public',
    'register',
    'reinterpret_cast',
    'requires',
    'return',
    'short',
    'signed',
    'sizeof',
    'static',
    'static_assert',
    'static_cast',
    'struct',
    'switch',
    'template',
    'this',
    'thread_local',
    'throw',
    'true',
    'try',
    'typedef',
    'typeid',
    'typename',
    'union',
    'unsigned',
    'using',
    'virtual',
    'void',
    'volatile',
    'wchar_t',
    'while',
    'xor',
    'xor_eq',
  };


  String get cppAccessor =>
      _cppReservedNames.contains(name) ? '${name}_' : name;
}

import '../field_type.dart';

/// A uint that is stored in memory as a uint16_t to save space.
///
/// On the wire it is identical to [UintFieldType]: the same varuint encoding
/// and the same core field-type id. Only the generated C++ member type differs
/// (uint16_t instead of uint32_t). [registryType] folds it back into `uint` for
/// registry dispatch (so existing KeyFrameUint keyframes still apply) and for
/// the property field-type id (so existing files remain compatible).
class Uint16FieldType extends FieldType {
  Uint16FieldType()
      : super(
          'uint16',
          'CoreUintType',
          cppName: 'uint16_t',
        );

  @override
  String get defaultValue => '0';

  // We do this to fix up CoreContext.invalidPropertyKey
  @override
  String? convertCpp(String value) =>
      value.replaceAll('CoreContext.', 'Core::');

  @override
  FieldType get registryType => FieldType.find('uint')!;
}

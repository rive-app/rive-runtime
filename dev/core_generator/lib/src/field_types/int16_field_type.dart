import '../field_type.dart';

/// An int that is stored in memory as an int16_t to save space.
///
/// On the wire it is identical to [IntFieldType]: the same zigzag varuint
/// encoding and the same core field-type id. Only the generated C++ member
/// type differs (int16_t instead of int32_t). [registryType] folds it back
/// into `int` for registry dispatch and for the property field-type id.
class Int16FieldType extends FieldType {
  Int16FieldType()
      : super(
          'int16',
          'CoreIntType',
          cppName: 'int16_t',
        );

  @override
  String get defaultValue => '0';

  // We do this to fix up CoreContext.invalidPropertyKey
  @override
  String? convertCpp(String value) =>
      value.replaceAll('CoreContext.', 'Core::');

  @override
  FieldType get registryType => FieldType.find('int')!;
}

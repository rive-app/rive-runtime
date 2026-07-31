import '../field_type.dart';

/// A signed integer, zigzag encoded so small negatives stay one byte.
///
/// On the wire it is a varuint, which is why it shares CoreUintType's field
/// type id — see core_int_type.hpp. Narrower aliases (e.g. int16) fold back
/// into `int` via [registryType].
class IntFieldType extends FieldType {
  IntFieldType()
      : super(
          'int',
          'CoreIntType',
          cppName: 'int32_t',
        );

  @override
  String get defaultValue => '0';

  // We do this to fix up CoreContext.invalidPropertyKey
  @override
  String? convertCpp(String value) =>
      value.replaceAll('CoreContext.', 'Core::');
}

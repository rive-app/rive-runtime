import '../field_type.dart';

class Uint64FieldType extends FieldType {
  Uint64FieldType()
      : super(
          'uint64',
          'CoreUint64Type',
          cppName: 'uint64_t',
        );

  @override
  String get defaultValue => '0';

  // We do this to fix up CoreContext.invalidProperyKey
  @override
  String? convertCpp(String value) =>
      value.replaceAll('CoreContext.', 'Core::');
}

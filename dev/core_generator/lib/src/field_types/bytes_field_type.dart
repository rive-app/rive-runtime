import '../field_type.dart';

class BytesFieldType extends FieldType {
  BytesFieldType()
      : super(
          'Bytes',
          'CoreBytesType',
          cppName: 'std::vector<uint8_t>',
          include: '<vector>',
        );

  @override
  String get defaultValue => 'std::vector<uint8_t>()';

  @override
  String get cppGetterName => 'const std::vector<uint8_t>&';

  @override
  String convertCpp(String value) {
    return null;
  }
}

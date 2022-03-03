import '../field_type.dart';

class BytesFieldType extends FieldType {
  BytesFieldType()
      : super(
          'Bytes',
          'CoreBytesType',
          cppName: 'const Span<uint8_t>&',
          include: 'rive/span.hpp',
        );

  @override
  String get defaultValue => 'Span<uint8_t>(nullptr, 0)';

  @override
  String get cppGetterName => 'const Span<uint8_t>&';

  @override
  String convertCpp(String value) {
    return null;
  }
}

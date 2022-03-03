import '../field_type.dart';

class BytesFieldType extends FieldType {
  BytesFieldType()
      : super(
          'Bytes',
          'CoreBytesType',
          cppName: 'Span<const uint8_t>',
          include: 'rive/span.hpp',
        );

  @override
  String get defaultValue => 'Span<const uint8_t>(nullptr, 0)';

  @override
  String get cppGetterName => 'Span<const uint8_t>';

  @override
  String convertCpp(String value) {
    return null;
  }
}

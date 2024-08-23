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
  String get defaultValue => 'Span()';

  @override
  String get cppGetterName => 'Span<const uint8_t>';

  @override
  String? convertCpp(String value) {
    return null;
  }
}

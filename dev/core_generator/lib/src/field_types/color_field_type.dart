import '../field_type.dart';

class ColorFieldType extends FieldType {
  ColorFieldType()
      : super(
          'Color',
          'CoreColorType',
          cppName: 'int',
        );

  @override
  String get defaultValue => '0';
}

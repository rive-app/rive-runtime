import '../field_type.dart';

class DoubleFieldType extends FieldType {
  DoubleFieldType()
      : super(
          'double',
          'CoreDoubleType',
          cppName: 'float'
        );

  @override
  String get defaultValue => '0.0f';
}

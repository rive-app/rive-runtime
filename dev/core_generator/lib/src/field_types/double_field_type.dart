import '../field_type.dart';

class DoubleFieldType extends FieldType {
  DoubleFieldType()
      : super(
          'double',
          'CoreDoubleType',
        );

  @override
  String get defaultValue => '0.0';
}

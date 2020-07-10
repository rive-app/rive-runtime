import '../field_type.dart';

class IntFieldType extends FieldType {
  IntFieldType()
      : super(
          'int',
          'CoreIntType',
        );

  @override
  String get defaultValue => '0';
}

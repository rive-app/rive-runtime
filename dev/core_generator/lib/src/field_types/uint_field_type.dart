import '../field_type.dart';

class UintFieldType extends FieldType {
  UintFieldType()
      : super(
          'uint',
          'CoreUintType',
          cppName: 'int',
        );

  @override
  String get defaultValue => '0';
}

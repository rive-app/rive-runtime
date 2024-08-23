import '../field_type.dart';

class BoolFieldType extends FieldType {
  BoolFieldType()
      : super(
          'bool',
          'CoreBoolType',
        );

  @override
  String get defaultValue => 'false';
}

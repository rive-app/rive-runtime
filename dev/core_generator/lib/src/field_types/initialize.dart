// All supported field types.

import 'package:core_generator/src/field_type.dart';

List<FieldType> fields;

void initializeFields() {
  fields = [
    StringFieldType(),
    UintFieldType(),
    DoubleFieldType(),
    BoolFieldType(),
    ColorFieldType(),
  ];
}

// All supported field types.

import 'package:core_generator/src/field_type.dart';
import 'package:core_generator/src/field_types/bytes_field_type.dart';

List<FieldType> fields;

void initializeFields() {
  fields = [
    StringFieldType(),
    BytesFieldType(),
    UintFieldType(),
    DoubleFieldType(),
    BoolFieldType(),
    ColorFieldType(),
  ];
}

// All supported field types.

import 'package:core_generator/src/field_type.dart';
import 'package:core_generator/src/field_types/bytes_field_type.dart';
import 'package:core_generator/src/field_types/callback_field_type.dart';
import 'package:core_generator/src/field_types/uint8_field_type.dart';

late List<FieldType> fields;

void initializeFields() {
  fields = [
    StringFieldType(),
    BytesFieldType(),
    UintFieldType(),
    Uint64FieldType(),
    // Must come after UintFieldType: its registryType resolves `uint`.
    Uint8FieldType(),
    DoubleFieldType(),
    BoolFieldType(),
    ColorFieldType(),
    CallbackFieldType(),
  ];
}

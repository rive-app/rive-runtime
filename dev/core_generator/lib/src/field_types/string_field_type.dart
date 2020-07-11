import '../field_type.dart';

class StringFieldType extends FieldType {
  StringFieldType()
      : super(
          'String',
          'CoreStringType',
          cppName: 'std::string',
          include: '<string>'
        );
}

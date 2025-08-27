import '../field_type.dart';

class StringFieldType extends FieldType {
  StringFieldType()
      : super('String', 'CoreStringType',
            cppName: 'std::string', include: '<string>');
  @override
  String get defaultValue => '""';

  @override
  String get cppGetterName => 'const std::string&';

  @override
  String? convertCpp(String value) {
    var result = value;
    if (result.length > 1) {
      if (result[0] == '\'') {
        result = '"' + result.substring(1);
      }
      if (result[result.length - 1] == '\'') {
        result = result.substring(0, result.length - 1) + '"';
      }
    }
    return result;
  }
}

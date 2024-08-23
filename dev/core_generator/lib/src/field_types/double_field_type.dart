import '../field_type.dart';

class DoubleFieldType extends FieldType {
  DoubleFieldType() : super('double', 'CoreDoubleType', cppName: 'float');

  @override
  String get defaultValue => '0.0f';

  @override
  String? convertCpp(String value) {
    var result = value;
    if (result.isNotEmpty) {
      if (result[result.length - 1] != 'f') {
        if (!result.contains('.')) {
          result += '.0';
        }
        result += 'f';
      }
    }
    return result;
  }
}

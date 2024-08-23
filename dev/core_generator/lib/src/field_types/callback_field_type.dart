import 'package:core_generator/src/field_type.dart';

class CallbackFieldType extends FieldType {
  CallbackFieldType()
      : super(
          'callback',
          'CoreCallbackType',
          cppName: 'CallbackData',
          storesData: false,
        );

  @override
  String get defaultValue => '0';
}

import 'definition.dart';
import 'property.dart';

class Key {
  final String? stringValue;
  final int? intValue;

  bool get isMissing => intValue == null;

  Key(this.stringValue, this.intValue);
  Key.forDefinition(Definition def)
      : stringValue = def.name?.toLowerCase(),
        intValue = null;
  Key.forProperty(Property field)
      : stringValue = field.name.toLowerCase(),
        intValue = null;

  Key withIntValue(int id) => Key(stringValue, id);

  static Key? fromJSON(dynamic data) {
    if (data is! Map<String, dynamic>) {
      return null;
    }
    dynamic iv = data['int'];
    dynamic sv = data['string'];
    if (iv is int && sv is String) {
      return Key(sv, iv);
    }
    return null;
  }

  Map<String, dynamic> serialize() =>
      <String, dynamic>{'int': intValue, 'string': stringValue};
}

import 'definition.dart';
import 'property.dart';

class Key {
  final String? stringValue;
  final int? intValue;
  final List<Key> alternates = [];

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
    dynamic av = data['alternates'];
    if (iv is int && sv is String) {
      final key = Key(sv, iv);
      if (av is List) {
        for (final a in av) {
          if (a is Map<String, dynamic>) {
            dynamic altiv = a['int'];
            dynamic altsv = a['string'];
            key.alternates.add(Key(altsv, altiv));
          }
        }
      }
      return key;
    }
    return null;
  }

  Map<String, dynamic> serialize() {
    final json = <String, dynamic>{'int': intValue, 'string': stringValue};
    final altsJson = [];
    for (final alt in alternates) {
      final altJson = <String, dynamic>{
        'int': alt.intValue,
        'string': alt.stringValue
      };
      altsJson.add(altJson);
    }
    if (altsJson.isNotEmpty) {
      json['alternates'] = altsJson;
    }
    return json;
  }
}

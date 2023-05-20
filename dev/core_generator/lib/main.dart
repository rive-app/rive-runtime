import 'dart:io';

import 'package:core_generator/src/configuration.dart';
import 'package:core_generator/src/definition.dart';
import 'package:core_generator/src/field_types/initialize.dart';

void main(List<String> arguments) {
  initializeFields();
  Directory(defsPath).list(recursive: true).listen(
    (entity) {
      if (entity is File && entity.path.toLowerCase().endsWith('.json')) {
        Definition.make(entity.path.substring(defsPath.length));
      }
    },
    onDone: Definition.generate,
  );
}

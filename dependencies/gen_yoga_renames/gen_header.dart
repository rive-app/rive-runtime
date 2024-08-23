import 'dart:collection';
import 'dart:io';

final skip = HashSet.from(
  [],
);

final extras = HashSet.from(
  [],
);

void main() {
  final uniqueNames = HashSet<String>();
  var header = StringBuffer();
  header.writeln('// clang-format off');
  header.writeln('// YG*');
  var contents = File('yoga_names.txt').readAsStringSync();
  RegExp exp = RegExp(r'\s(YG([a-zA-Z0-9_]*))', multiLine: true);
  Iterable<RegExpMatch> matches = exp.allMatches(contents);
  for (final m in matches) {
    var symbolName = m[1];
    if (symbolName == null ||
        skip.contains(symbolName) ||
        uniqueNames.contains(symbolName)) {
      continue;
    }
    uniqueNames.add(symbolName);
    header.writeln('#define $symbolName rive_$symbolName');
  }
  header.writeln('// _YG*');
  {
    RegExp exp = RegExp(r'\s_(YG([a-zA-Z0-9_]*))', multiLine: true);
    Iterable<RegExpMatch> matches = exp.allMatches(contents);
    for (final m in matches) {
      var symbolName = m[1];
      if (symbolName == null ||
          skip.contains(symbolName) ||
          uniqueNames.contains(symbolName)) {
        continue;
      }
      uniqueNames.add(symbolName);
      header.writeln('#define $symbolName rive_$symbolName');
    }
  }
  File('../rive_yoga_renames.h').writeAsStringSync(header.toString());
}

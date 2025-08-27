import 'dart:collection';
import 'dart:io';

final skip = HashSet.from(
  [
    'hb_color_get_alpha',
    'hb_color_get_green',
    'hb_color_get_blue',
    'hb_color_get_red',
    'hb_glyph_info_get_glyph_flags',
    'hb_declval',
  ],
);

final extras = HashSet.from(
  [
    'lookup_standard_encoding_for_code',
    'lookup_expert_encoding_for_code',
    'lookup_expert_charset_for_sid',
    'lookup_expert_subset_charset_for_sid',
    'lookup_standard_encoding_for_sid',
    'accelerator_t',
    'get_seac_components',
    'data_destroy_arabic',
  ],
);

void main() {
  final uniqueNames = HashSet<String>();
  var header = StringBuffer();
  header.writeln('// clang-format off');
  header.writeln('// hb_*');
  var contents = File('harfbuzz_names.txt').readAsStringSync();
  RegExp exp = RegExp(r'\s(hb_([a-zA-Z0-9_]*))', multiLine: true);
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
  header.writeln('// _hb_*');
  {
    RegExp exp = RegExp(r'\s_(hb_([a-zA-Z0-9_]*))', multiLine: true);
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
  header.writeln('// __hb_*');
  {
    RegExp exp = RegExp(r'\s_(_hb_([a-zA-Z0-9_]*))', multiLine: true);
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
  for (final symbolName in extras) {
    header.writeln('#define $symbolName rive_$symbolName');
  }
  File('../rive_harfbuzz_renames.h').writeAsStringSync(header.toString());
}

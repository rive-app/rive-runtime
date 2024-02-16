import 'dart:collection';
import 'dart:io';

var skip = HashSet.from(
  [
    'hb_color_get_alpha',
    'hb_color_get_green',
    'hb_color_get_blue',
    'hb_color_get_red',
    'hb_glyph_info_get_glyph_flags',
  ],
);

void main() {
  var header = StringBuffer();
  header.writeln('// clang-format off');
  var contents = File('harfbuzz_names.txt').readAsStringSync();
  RegExp exp = RegExp(r'\s_(hb_([a-zA-Z0-9_]*))$', multiLine: true);
  Iterable<RegExpMatch> matches = exp.allMatches(contents);
  for (final m in matches) {
    var symbolName = m[1];
    if (skip.contains(symbolName)) {
      continue;
    }
    header.writeln('#define $symbolName rive_$symbolName');
  }
  File('../rive_harfbuzz_renames.h').writeAsStringSync(header.toString());
}

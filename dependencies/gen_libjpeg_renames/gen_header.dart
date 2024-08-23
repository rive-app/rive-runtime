import 'dart:collection';
import 'dart:io';

final skip = HashSet.from(
  [],
);

final extras = HashSet.from(
  [
    'read_quant_tables',
    'read_scan_script',
    'set_quality_ratings',
    'set_quant_slots',
    'set_sample_factors',
    'read_color_map',
    'enable_signal_catcher',
    'start_progress_monitor',
    'end_progress_monitor',
    'read_stdin',
    'write_stdout',
    'jdiv_round_up',
    'jround_up',
    'jzero_far',
    'jcopy_sample_rows',
    'jcopy_block_row',
    'jtransform_parse_crop_spec',
    'jtransform_request_workspace',
    'jtransform_adjust_parameters',
    'jtransform_execute_transform',
    'jtransform_perfect_transform',
    'jcopy_markers_setup',
    'jcopy_markers_execute',
  ],
);

void main() {
  final uniqueNames = HashSet<String>();
  var header = StringBuffer();
  header.writeln('// clang-format off');
  header.writeln('// jpeg_*');
  var contents = File('libjpeg_names.txt').readAsStringSync();
  RegExp exp = RegExp(r'\s_(jpeg_([a-zA-Z0-9_]*))', multiLine: true);
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
  header.writeln('// jinit_*');
  {
    RegExp exp = RegExp(r'\s_(jinit_([a-zA-Z0-9_]*))', multiLine: true);
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
  header.writeln('// _j extras');
  for (final symbolName in extras) {
    header.writeln('#define $symbolName rive_$symbolName');
  }
  File('../rive_libjpeg_renames.h').writeAsStringSync(header.toString());
}

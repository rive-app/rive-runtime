RegExp _makeWrappingRegExp(int width) {
  //return RegExp('(?![^\n]{1,$width}\$)([^\n]{1,$width})\s');
  return RegExp('(.{1,$width})( +|\$\n?)|(.{1,$width})');
}

Map<int, RegExp> _indentRegexp = {};
String comment(String s, {int indent = 0, bool doubleSlashes = false}) {
  final slashes = doubleSlashes ? '//' : '///';
  var reg = _indentRegexp[indent];
  if (reg == null) {
    _indentRegexp[indent] = reg = _makeWrappingRegExp(80 - 4 - 2 * indent);
  }

  return '$slashes ' +
      s
          .replaceAllMapped(reg, (Match m) => '${m[1]}${m[2]}\n')
          .trim()
          .split('\n')
          .join('\n  $slashes ') +
      '\n';
}

String capitalize(String s) => s[0].toUpperCase() + s.substring(1);

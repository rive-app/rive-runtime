import 'dart:convert';
import 'dart:io';

class CppFormatter {
  Future<String> format(String code) async {
    var process = await Process.start('clang-format', []);
    process.stdin.write(code);
    await process.stdin.close();
    return utf8.decodeStream(process.stdout);
  }

  /// Formats a fragment destined for a class body. clang-format needs a
  /// complete declaration, so the fragment rides inside a throwaway class
  /// whose wrapper lines are stripped back off.
  Future<String> formatClassBody(String code) async {
    var formatted = await format('class _RiveEditorExtension\n{\n$code\n};');
    var lines = formatted.split('\n');
    while (lines.isNotEmpty && lines.first.trim() != '{') {
      lines.removeAt(0);
    }
    if (lines.isNotEmpty) {
      lines.removeAt(0);
    }
    while (lines.isNotEmpty && lines.last.trim().isEmpty) {
      lines.removeLast();
    }
    if (lines.isNotEmpty && lines.last.trim() == '};') {
      lines.removeLast();
    }
    return '${lines.join('\n')}\n';
  }

  Future<String> formatAndGuard(String name, String code) async {
    String guardName = name
        .replaceAllMapped(
            RegExp('(.+?)([A-Z])'), (Match m) => '${m[1]}_${m[2]}')
        .toUpperCase();
    return format('''#ifndef _RIVE_${guardName}_HPP_
        #define _RIVE_${guardName}_HPP_
        $code
        #endif''');
  }
}

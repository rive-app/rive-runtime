import 'dart:convert';
import 'dart:io';

class CppFormatter {
  Future<String> format(String code) async {
    var process = await Process.start('clang-format', []);
    process.stdin.write(code);
    await process.stdin.close();
    return utf8.decodeStream(process.stdout);
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

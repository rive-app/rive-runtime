#ifndef _RIVE_STRING_TEMPLATE_FORMAT_HPP_
#define _RIVE_STRING_TEMPLATE_FORMAT_HPP_
#include <cstdint>
#include <string>
#include <vector>

namespace rive
{
// A piece of a parsed string template: literal text or a placeholder.
struct StringTemplateSegment
{
  enum class Kind : uint8_t
  {
    literal,
    // ${name}: the first argument with that name.
    name,
    // ${2}: the argument at that position.
    position,
    // ${}: the next argument, counting only these placeholders.
    next,
  };
  Kind kind = Kind::literal;
  // Literal text, or the placeholder's source text (e.g. "${score:%.1f}").
  std::string text;
  std::string name;
  // The argument position for position and next placeholders.
  uint32_t index = 0;
  std::string spec;

  bool isPlaceholder() const { return kind != Kind::literal; }
};

// Parsing and printf-style formatting for DataConverterStringTemplate. The
// Dart implementation in rive_core (string_template_parser.dart) must parse
// identically, and the editor formats numbers through formatNumber via
// rive_native so both produce the same text.
class StringTemplateFormat
{
public:
  // Splits a template into literal and placeholder segments.
  //  - A placeholder is `open key close` or `open key:spec close`, where key
  //    is a name ([A-Za-z_][A-Za-z0-9_]*), an index (digits) or empty, and
  //    spec is any text up to the next `close`.
  //  - Empty placeholders are numbered 0, 1, 2... in the order they appear.
  //  - A backslash followed by `open` writes `open`, and two backslashes
  //    write one; any other backslash is literal.
  //  - Anything that doesn't start a valid placeholder is literal text.
  //  - With an invalid delimiter the whole template is literal text.
  static void parse(
      const std::string& templateText,
      const std::string& open,
      const std::string& close,
      std::vector<StringTemplateSegment>& segments);

  // Delimiters are 1 to 4 printable ASCII symbols, excluding ':' (starts a
  // spec), the backslash (escapes) and '_' (part of names).
  static bool isValidDelimiter(const std::string& delimiter);

  // Formats a number with a printf-style spec:
  //   %?[flags -+ 0#,][width 0-99][.precision 0-99][conversion diuxXofFeEgG]
  // `,` adds thousands separators. An empty or invalid spec uses the
  // default format: up to 6 decimals with trailing zeros removed. NaN and
  // infinities are always "nan", "inf" and "-inf".
  static std::string formatNumber(float value, const std::string& spec);

  static bool isValidNumberSpec(const std::string& spec);

  // Inserts a comma every three digits of the integer part of a formatted
  // number, e.g. "-1234567.5" -> "-1,234,567.5".
  static std::string formatThousands(const std::string& number);
};
} // namespace rive

#endif

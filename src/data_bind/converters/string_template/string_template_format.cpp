#include "rive/data_bind/converters/string_template/string_template_format.hpp"
#include "rive/data_bind/converters/data_converter_string_remove_zeros.hpp"
#include <cmath>
#include <cstdio>

using namespace rive;

namespace
{
bool isDigit(char c) { return c >= '0' && c <= '9'; }

bool isNameStart(char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool isNameChar(char c) { return isNameStart(c) || isDigit(c); }

bool startsWith(
    const std::string& text,
    size_t index,
    const std::string& prefix)
{
  return index <= text.size() && text.size() - index >= prefix.size() &&
         text.compare(index, prefix.size(), prefix) == 0;
}

bool isIntegerConversion(char c)
{
  return c == 'd' || c == 'i' || c == 'u' || c == 'x' || c == 'X' || c == 'o';
}

bool isFloatConversion(char c)
{
  return c == 'f' || c == 'F' || c == 'e' || c == 'E' || c == 'g' || c == 'G';
}

struct NumberSpec
{
  bool leftAlign = false;
  bool plus = false;
  bool space = false;
  bool zeroPad = false;
  bool alternate = false;
  bool thousands = false;
  int width = -1;
  int precision = -1;
  char conversion = 0;
};

// Reads up to two digits at [index]. Returns false if a third digit follows.
bool readTwoDigits(const std::string& spec, size_t& index, int& value)
{
  size_t start = index;
  int result = 0;
  while (index < spec.size() && isDigit(spec[index]) && index - start < 2)
  {
    result = result * 10 + (spec[index] - '0');
    index++;
  }
  if (index > start)
  {
    value = result;
  }
  return index >= spec.size() || !isDigit(spec[index]);
}

bool parseNumberSpec(const std::string& spec, NumberSpec& out)
{
  size_t index = 0;
  if (index < spec.size() && spec[index] == '%')
  {
    index++;
  }
  for (; index < spec.size(); index++)
  {
    char c = spec[index];
    if (c == '-')
    {
      out.leftAlign = true;
    }
    else if (c == '+')
    {
      out.plus = true;
    }
    else if (c == ' ')
    {
      out.space = true;
    }
    else if (c == '0')
    {
      out.zeroPad = true;
    }
    else if (c == '#')
    {
      out.alternate = true;
    }
    else if (c == ',')
    {
      out.thousands = true;
    }
    else
    {
      break;
    }
  }
  if (!readTwoDigits(spec, index, out.width))
  {
    return false;
  }
  if (index < spec.size() && spec[index] == '.')
  {
    index++;
    out.precision = 0;
    if (!readTwoDigits(spec, index, out.precision))
    {
      return false;
    }
  }
  if (index + 1 != spec.size())
  {
    return false;
  }
  char conversion = spec[index];
  if (!isIntegerConversion(conversion) && !isFloatConversion(conversion))
  {
    return false;
  }
  out.conversion = conversion;
  bool isDecimal = conversion == 'd' || conversion == 'i' || conversion == 'u';
  // printf leaves '#' undefined for decimal integers.
  if (out.alternate && isDecimal)
  {
    return false;
  }
  // Separators only make sense in decimal output, and zero padding would
  // put zeros between the separators.
  if (out.thousands &&
      (out.zeroPad || !(isDecimal || conversion == 'f' || conversion == 'F' ||
                        conversion == 'g' || conversion == 'G')))
  {
    return false;
  }
  return true;
}

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
// The format is rebuilt from a validated NumberSpec, never taken from user
// text, and always matches the type of value.
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
template <typename T> std::string print(const std::string& format, T value)
{
  int length = snprintf(nullptr, 0, format.c_str(), value);
  if (length <= 0)
  {
    return "";
  }
  std::string output(static_cast<size_t>(length), '\0');
  snprintf(&output[0], static_cast<size_t>(length) + 1, format.c_str(), value);
  return output;
}
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

std::string formatDefault(float value)
{
  auto output = DataConverterStringRemoveZeros::removeZeros(
      print("%f", (double)value));
  // Values that round to zero keep their sign in printf ("-0.000000").
  if (output == "-0")
  {
    return "0";
  }
  return output;
}
} // namespace

bool StringTemplateFormat::isValidDelimiter(const std::string& delimiter)
{
  if (delimiter.empty() || delimiter.size() > 4)
  {
    return false;
  }
  for (char c : delimiter)
  {
    // Printable ASCII punctuation, minus ':' which starts a spec, the
    // backslash which escapes, and '_' which is part of names. Bytes above
    // 127 are negative and fall outside.
    bool isPunctuation = (c >= '!' && c <= '/') || (c >= ';' && c <= '@') ||
                         (c >= '[' && c <= '`') || (c >= '{' && c <= '~');
    if (!isPunctuation || c == '\\' || c == '_')
    {
      return false;
    }
  }
  return true;
}

void StringTemplateFormat::parse(
    const std::string& templateText,
    const std::string& open,
    const std::string& close,
    std::vector<StringTemplateSegment>& segments)
{
  segments.clear();
  if (!isValidDelimiter(open) || !isValidDelimiter(close))
  {
    // Nothing can be a placeholder.
    if (!templateText.empty())
    {
      StringTemplateSegment segment;
      segment.text = templateText;
      segments.push_back(segment);
    }
    return;
  }
  std::string literal;
  uint32_t nextIndex = 0;
  // Where a search for `close` already failed: no later search can succeed,
  // so a run of unterminated `${a:` stays linear instead of rescanning the
  // rest of the text for each one.
  size_t closeMissingFrom = std::string::npos;
  size_t size = templateText.size();
  size_t index = 0;
  while (index < size)
  {
    char c = templateText[index];
    if (c == '\\')
    {
      if (startsWith(templateText, index + 1, open))
      {
        literal += open;
        index += 1 + open.size();
        continue;
      }
      if (index + 1 < size && templateText[index + 1] == '\\')
      {
        literal += '\\';
        index += 2;
        continue;
      }
    }
    if (startsWith(templateText, index, open))
    {
      size_t keyStart = index + open.size();
      size_t keyEnd = keyStart;
      auto kind = StringTemplateSegment::Kind::next;
      uint32_t position = 0;
      if (keyEnd < size && isNameStart(templateText[keyEnd]))
      {
        kind = StringTemplateSegment::Kind::name;
        while (keyEnd < size && isNameChar(templateText[keyEnd]))
        {
          keyEnd++;
        }
      }
      else if (keyEnd < size && isDigit(templateText[keyEnd]))
      {
        kind = StringTemplateSegment::Kind::position;
        while (keyEnd < size && isDigit(templateText[keyEnd]))
        {
          // Saturates: an index this large is past any argument.
          if (position < 100000000)
          {
            position = position * 10 + (templateText[keyEnd] - '0');
          }
          keyEnd++;
        }
      }
      size_t specStart = std::string::npos;
      size_t closeStart = std::string::npos;
      if (startsWith(templateText, keyEnd, close))
      {
        closeStart = keyEnd;
      }
      else if (keyEnd < size && templateText[keyEnd] == ':')
      {
        specStart = keyEnd + 1;
        if (closeMissingFrom == std::string::npos ||
            specStart < closeMissingFrom)
        {
          closeStart = templateText.find(close, specStart);
          if (closeStart == std::string::npos)
          {
            closeMissingFrom = specStart;
          }
        }
      }
      if (closeStart != std::string::npos)
      {
        if (!literal.empty())
        {
          StringTemplateSegment segment;
          segment.text = literal;
          segments.push_back(segment);
          literal.clear();
        }
        size_t end = closeStart + close.size();
        StringTemplateSegment placeholder;
        placeholder.kind = kind;
        placeholder.text = templateText.substr(index, end - index);
        switch (kind)
        {
          case StringTemplateSegment::Kind::name:
            placeholder.name = templateText.substr(keyStart, keyEnd - keyStart);
            break;
          case StringTemplateSegment::Kind::position:
            placeholder.index = position;
            break;
          default:
            placeholder.index = nextIndex++;
            break;
        }
        if (specStart != std::string::npos)
        {
          placeholder.spec = templateText.substr(
              specStart,
              closeStart - specStart);
        }
        segments.push_back(placeholder);
        index = end;
        continue;
      }
    }
    literal += c;
    index++;
  }
  if (!literal.empty())
  {
    StringTemplateSegment segment;
    segment.text = literal;
    segments.push_back(segment);
  }
}

bool StringTemplateFormat::isValidNumberSpec(const std::string& spec)
{
  NumberSpec numberSpec;
  return parseNumberSpec(spec, numberSpec);
}

std::string StringTemplateFormat::formatNumber(
    float value,
    const std::string& spec)
{
  if (!std::isfinite(value))
  {
    // Spelled here because printf's spelling differs per platform (e.g.
    // "-nan(ind)" on Windows).
    return std::isnan(value) ? "nan" : (value < 0.0f ? "-inf" : "inf");
  }
  NumberSpec numberSpec;
  if (spec.empty() || !parseNumberSpec(spec, numberSpec))
  {
    return formatDefault(value);
  }
  char conversion = numberSpec.conversion;
  bool isInteger = isIntegerConversion(conversion);

  std::string format = "%";
  if (numberSpec.leftAlign)
  {
    format += '-';
  }
  if (numberSpec.plus)
  {
    format += '+';
  }
  if (numberSpec.space)
  {
    format += ' ';
  }
  if (numberSpec.zeroPad)
  {
    format += '0';
  }
  if (numberSpec.alternate)
  {
    format += '#';
  }
  // With separators the width is applied after inserting them.
  if (numberSpec.width >= 0 && !numberSpec.thousands)
  {
    format += std::to_string(numberSpec.width);
  }
  if (numberSpec.precision >= 0)
  {
    format += '.';
    format += std::to_string(numberSpec.precision);
  }
  if (isInteger)
  {
    format += "ll";
  }
  format += conversion;

  std::string output;
  if (isInteger)
  {
    // Round like the To String converter and stay inside long long.
    constexpr double limit = 9.0e18;
    double rounded = std::round((double)value);
    rounded = rounded > limit ? limit : (rounded < -limit ? -limit : rounded);
    if (conversion == 'd' || conversion == 'i')
    {
      output = print(format, static_cast<long long>(rounded));
    }
    else
    {
      output = print(
          format,
          static_cast<unsigned long long>(rounded < 0.0 ? 0.0 : rounded));
    }
  }
  else
  {
    output = print(format, (double)value);
  }

  if (numberSpec.thousands)
  {
    output = formatThousands(output);
    size_t width = static_cast<size_t>(
        numberSpec.width < 0 ? 0 : numberSpec.width);
    if (output.size() < width)
    {
      std::string padding(width - output.size(), ' ');
      output = numberSpec.leftAlign ? output + padding : padding + output;
    }
  }
  return output;
}

std::string StringTemplateFormat::formatThousands(const std::string& number)
{
  // Split integer and fractional parts
  std::string intPart, fracPart;
  size_t dotPos = number.find('.');
  if (dotPos != std::string::npos)
  {
    intPart = number.substr(0, dotPos);
    fracPart = number.substr(dotPos); // keep the '.' too
  }
  else
  {
    intPart = number;
  }

  // Insert commas into the integer part
  int insertPosition = (int)intPart.length() - 3;
  while (insertPosition > 0 && isDigit(intPart[insertPosition - 1]))
  {
    intPart.insert(insertPosition, ",");
    insertPosition -= 3;
  }

  return intPart + fracPart;
}

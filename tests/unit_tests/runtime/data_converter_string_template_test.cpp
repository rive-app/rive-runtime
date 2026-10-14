#include "rive/data_bind/converters/data_converter_string_template.hpp"
#include "rive/data_bind/converters/string_template/string_template_argument_number.hpp"
#include "rive/data_bind/converters/string_template/string_template_argument_string.hpp"
#include "rive/data_bind/converters/string_template/string_template_format.hpp"
#include "rive/data_bind/data_bind_context.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include "rive/custom_property_string.hpp"
#include "rive/file.hpp"
#include "rive/generated/backboard_base.hpp"
#include "rive/data_bind/converters/data_converter_rounder.hpp"
#include "rive/generated/data_bind/data_bind_base.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_number_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_string_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_runtime.hpp"
#include "riv_bytes.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <limits>

using namespace rive;

namespace
{
std::string describe(const std::vector<StringTemplateSegment>& segments)
{
    std::string result;
    for (const auto& segment : segments)
    {
        if (segment.isPlaceholder())
        {
            switch (segment.kind)
            {
                case StringTemplateSegment::Kind::position:
                    result += "[#" + std::to_string(segment.index);
                    break;
                case StringTemplateSegment::Kind::next:
                    result += "[+" + std::to_string(segment.index);
                    break;
                default:
                    result += "[" + segment.name;
                    break;
            }
            if (!segment.spec.empty())
            {
                result += ":" + segment.spec;
            }
            result += "]";
        }
        else
        {
            result += "<" + segment.text + ">";
        }
    }
    return result;
}

std::string parse(const std::string& text,
                  const std::string& open = "${",
                  const std::string& close = "}")
{
    std::vector<StringTemplateSegment> segments;
    StringTemplateFormat::parse(text, open, close, segments);
    return describe(segments);
}

StringTemplateArgumentNumber* numberArgument(const std::string& name,
                                             float value)
{
    auto argument = new StringTemplateArgumentNumber();
    argument->name(name);
    argument->propertyValue(value);
    return argument;
}

StringTemplateArgumentString* stringArgument(const std::string& name,
                                             const std::string& value)
{
    auto argument = new StringTemplateArgumentString();
    argument->name(name);
    argument->propertyValue(value);
    return argument;
}

std::string convert(DataConverterStringTemplate& converter,
                    const std::string& templateText)
{
    DataValueString input(templateText);
    auto output = converter.convert(&input, nullptr);
    REQUIRE(output->is<DataValueString>());
    return output->as<DataValueString>()->value();
}
} // namespace

TEST_CASE("string template parses placeholders", "[string_template]")
{
    CHECK(parse("You got ${numerical_score} right") ==
          "<You got >[numerical_score]< right>");
    CHECK(parse("${a:%.1f}") == "[a:%.1f]");
    CHECK(parse("${a}${b}") == "[a][b]");
    CHECK(parse("${_a1:x}") == "[_a1:x]");
    // An empty spec is no spec.
    CHECK(parse("${a:}") == "[a]");
    CHECK(parse("") == "");
}

TEST_CASE("string template placeholders by index and by order",
          "[string_template]")
{
    CHECK(parse("${1} scored ${0:%.1f}") == "[#1]< scored >[#0:%.1f]");
    CHECK(parse("${} and ${}") == "[+0]< and >[+1]");
    CHECK(parse("${:%d}") == "[+0:%d]");
    CHECK(parse("${01}") == "[#1]");
    // Only empty placeholders count towards ${}.
    CHECK(parse("${} ${name} ${5} ${}") == "[+0]< >[name]< >[#5]< >[+1]");
    // Indexes too large for any argument saturate.
    CHECK(parse("${99999999999}") == "[#999999999]");
    CHECK(parse("${1a}") == "<${1a}>");
    CHECK(parse("${-1}") == "<${-1}>");
    // With the same symbol on both sides an empty pair is a placeholder.
    CHECK(parse("50%% off", "%", "%") == "<50>[+0]< off>");
    CHECK(parse("50\\%% off", "%", "%") == "<50%% off>");
}

TEST_CASE("string template keeps what isn't a placeholder as text",
          "[string_template]")
{
    CHECK(parse("${1a}") == "<${1a}>");
    CHECK(parse("${a b}") == "<${a b}>");
    CHECK(parse("${ }") == "<${ }>");
    CHECK(parse("${a") == "<${a>");
    CHECK(parse("${a:%d") == "<${a:%d>");
    CHECK(parse("$a {b}") == "<$a {b}>");
    CHECK(parse("${") == "<${>");
    CHECK(parse("$") == "<$>");
    CHECK(parse("costs $5") == "<costs $5>");
    // UTF-8 passes through untouched.
    CHECK(parse("¡Hola ${nombre}! ✓") == "<¡Hola >[nombre]<! ✓>");
}

TEST_CASE("string template escapes", "[string_template]")
{
    CHECK(parse("\\${a}") == "<${a}>");
    CHECK(parse("\\\\${a}") == "<\\>[a]");
    CHECK(parse("C:\\path\\") == "<C:\\path\\>");
    CHECK(parse("\\n") == "<\\n>");
    // Only the whole opening delimiter is escaped.
    CHECK(parse("\\$5") == "<\\$5>");
}

TEST_CASE("string template with other delimiters", "[string_template]")
{
    // {name}
    CHECK(parse("You got {score} right", "{", "}") ==
          "<You got >[score]< right>");
    CHECK(parse("{a:%.1f}", "{", "}") == "[a:%.1f]");
    CHECK(parse("\\{a}", "{", "}") == "<{a}>");
    CHECK(parse("{{a}}", "{", "}") == "<{>[a]<}>");
    CHECK(parse("{ a } {1}", "{", "}") == "<{ a } >[#1]");
    CHECK(parse("${a}", "{", "}") == "<$>[a]");

    // {{name}}
    CHECK(parse("{{a}} {{b:%d}}", "{{", "}}") == "[a]< >[b:%d]");
    CHECK(parse("{a}", "{{", "}}") == "<{a}>");
    CHECK(parse("{{{a}}}", "{{", "}}") == "<{>[a]<}>");
    CHECK(parse("\\{{a}}", "{{", "}}") == "<{{a}}>");

    // The same symbol on both sides.
    CHECK(parse("%a% of %b:.1f%", "%", "%") == "[a]< of >[b:.1f]");
    CHECK(parse("100% sure", "%", "%") == "<100% sure>");
    CHECK(parse("\\%a%", "%", "%") == "<%a%>");
    // The spec ends at the first closing delimiter.
    CHECK(parse("%a:%.1f%", "%", "%") == "[a]<.1f%>");

    CHECK(parse("<a> & <b>", "<", ">") == "[a]< & >[b]");
    CHECK(parse("[[a]]", "[[", "]]") == "[a]");
    CHECK(parse("@@a@@", "@@", "@@") == "[a]");
}

TEST_CASE("string template delimiters must be ASCII symbols",
          "[string_template]")
{
    for (const char* delimiter :
         {"${", "{", "}}", "%", "<", "[[", "@@", "~~~~", "!", "`", "$"})
    {
        INFO(delimiter);
        CHECK(StringTemplateFormat::isValidDelimiter(delimiter));
    }
    for (const char* delimiter : {"",
                                  "a",
                                  "A",
                                  "1",
                                  "_",
                                  ":",
                                  "\\",
                                  " ",
                                  "\t",
                                  "{{{{{",
                                  "$a",
                                  "\xc2\xab"})
    {
        INFO(delimiter);
        CHECK_FALSE(StringTemplateFormat::isValidDelimiter(delimiter));
        // Nothing is a placeholder with an invalid delimiter.
        CHECK(parse("${a} {a}", delimiter, "}") == "<${a} {a}>");
        CHECK(parse("${a} {a}", "${", delimiter) == "<${a} {a}>");
    }
    CHECK(parse("", "", "") == "");
}

// Every unterminated `${a:` used to search the rest of the text for a closing
// delimiter, so a long run of them took quadratic time (1.8s for this input
// in a release build, ~7ms once failed searches are remembered).
TEST_CASE("string template parses long malformed input", "[string_template]")
{
    std::string text;
    for (int i = 0; i < 200000; i++)
    {
        text += "${a:";
    }
    CHECK(parse(text) == "<" + text + ">");
    // A close after the run still ends the first placeholder.
    CHECK(parse(text + "}${b}") == "[a:" + text.substr(4) + "][b]");
}

TEST_CASE("string template default number format", "[string_template]")
{
    CHECK(StringTemplateFormat::formatNumber(7.0f, "") == "7");
    CHECK(StringTemplateFormat::formatNumber(0.5f, "") == "0.5");
    CHECK(StringTemplateFormat::formatNumber(-12.25f, "") == "-12.25");
    CHECK(StringTemplateFormat::formatNumber(1.0f / 3.0f, "") == "0.333333");
    CHECK(StringTemplateFormat::formatNumber(0.0f, "") == "0");
    CHECK(StringTemplateFormat::formatNumber(-0.0f, "") == "0");
    CHECK(StringTemplateFormat::formatNumber(-0.0000001f, "") == "0");
    CHECK(StringTemplateFormat::formatNumber(1234567.0f, "") == "1234567");
}

TEST_CASE("string template printf number specs", "[string_template]")
{
    auto format = StringTemplateFormat::formatNumber;
    CHECK(format(3.14159f, "%.2f") == "3.14");
    CHECK(format(3.14159f, ".2f") == "3.14");
    CHECK(format(3.0f, "%.0f") == "3");
    CHECK(format(3.5f, "%6.2f") == "  3.50");
    CHECK(format(3.5f, "%-6.1f") == "3.5   ");
    CHECK(format(3.5f, "%+.1f") == "+3.5");
    CHECK(format(3.5f, "% .1f") == " 3.5");
    CHECK(format(3.5f, "%07.2f") == "0003.50");
    CHECK(format(2.0f, "%#.0f") == "2.");
    CHECK(format(1500000.0f, "%e") == "1.500000e+06");
    CHECK(format(1500000.0f, "%.2E") == "1.50E+06");
    CHECK(format(0.0001f, "%g") == "0.0001");
    CHECK(format(2.5f, "%F") == "2.500000");

    // Integers round rather than truncate.
    CHECK(format(2.5f, "%d") == "3");
    CHECK(format(2.4f, "%i") == "2");
    CHECK(format(-2.5f, "%d") == "-3");
    CHECK(format(42.0f, "%05d") == "00042");
    CHECK(format(42.0f, "%-5d") == "42   ");
    CHECK(format(42.0f, "%+d") == "+42");
    CHECK(format(7.0f, "%.3d") == "007");
    CHECK(format(255.0f, "%x") == "ff");
    CHECK(format(255.0f, "%X") == "FF");
    CHECK(format(255.0f, "%#x") == "0xff");
    CHECK(format(255.0f, "%o") == "377");
    CHECK(format(255.0f, "%u") == "255");
    // Unsigned conversions don't wrap negatives around.
    CHECK(format(-5.0f, "%u") == "0");
    CHECK(format(-5.0f, "%x") == "0");
    // Out of range values clamp instead of overflowing.
    CHECK(format(1.0e30f, "%d") == "9000000000000000000");
    CHECK(format(-1.0e30f, "%d") == "-9000000000000000000");
}

TEST_CASE("string template thousands separators", "[string_template]")
{
    auto format = StringTemplateFormat::formatNumber;
    CHECK(format(1234567.0f, "%,d") == "1,234,567");
    CHECK(format(-1234.0f, "%,d") == "-1,234");
    CHECK(format(999.0f, "%,d") == "999");
    CHECK(format(1234567.5f, "%,.1f") == "1,234,567.5");
    CHECK(format(1234.0f, "%,+d") == "+1,234");
    // Width is applied after the separators, with spaces.
    CHECK(format(1234.0f, "%,8d") == "   1,234");
    CHECK(format(1234.0f, "%-,8d") == "1,234   ");
    CHECK(format(1234.0f, "%,3d") == "1,234");
}

TEST_CASE("string template invalid specs use the default format",
          "[string_template]")
{
    auto format = StringTemplateFormat::formatNumber;
    for (const char* spec : {"%s",
                             "%n",
                             "%p",
                             "%c",
                             "%",
                             "%5",
                             "%dd",
                             "%ld",
                             "%lld",
                             "%*d",
                             "%100d",
                             "%.100f",
                             "%#d",
                             "%#u",
                             "%,x",
                             "%,e",
                             "%0,d",
                             "abc",
                             "%%"})
    {
        INFO(spec);
        CHECK_FALSE(StringTemplateFormat::isValidNumberSpec(spec));
        CHECK(format(7.5f, spec) == "7.5");
    }
    CHECK(StringTemplateFormat::isValidNumberSpec("%99.99f"));
    CHECK(StringTemplateFormat::isValidNumberSpec("d"));
}

TEST_CASE("string template non finite numbers", "[string_template]")
{
    auto format = StringTemplateFormat::formatNumber;
    float nan = std::numeric_limits<float>::quiet_NaN();
    float inf = std::numeric_limits<float>::infinity();
    CHECK(format(nan, "") == "nan");
    CHECK(format(-nan, "%d") == "nan");
    CHECK(format(inf, "%.1f") == "inf");
    CHECK(format(-inf, "%,d") == "-inf");
}

TEST_CASE("string template converter replaces placeholders",
          "[string_template]")
{
    DataConverterStringTemplate converter;
    converter.addArgument(stringArgument("player", "Ana"));
    converter.addArgument(numberArgument("score", 6.6f));
    converter.addArgument(numberArgument("total", 10.0f));
    CHECK(converter.outputType() == DataType::string);
    CHECK(convert(converter, "${player} got ${score:%d} of ${total} right") ==
          "Ana got 7 of 10 right");

    // A spec on a string argument is ignored.
    CHECK(convert(converter, "${player:%5d}!") == "Ana!");

    // Placeholders without an argument write nothing.
    CHECK(convert(converter, "[${missing:%.1f}] and ${score}") == "[] and 6.6");

    // Argument values are read on every convert.
    converter.arguments()[1]->as<StringTemplateArgumentNumber>()->propertyValue(
        12.0f);
    CHECK(convert(converter, "${score}") == "12");

    // No template, no text.
    CHECK(convert(converter, "") == "");
}

TEST_CASE("string template converter matches by index, order and name",
          "[string_template]")
{
    DataConverterStringTemplate converter;
    converter.addArgument(numberArgument("score", 7.25f));
    converter.addArgument(stringArgument("player", "Ana"));

    CHECK(convert(converter, "${1} scored ${0:%.1f}") == "Ana scored 7.2");
    CHECK(convert(converter, "${0}${0}") == "7.257.25");
    CHECK(convert(converter, "${} and ${}") == "7.25 and Ana");
    // More placeholders than arguments: the rest are empty.
    CHECK(convert(converter, "${}|${}|${}|${5}") == "7.25|Ana||");
    // All three in one template.
    CHECK(convert(converter, "${player}: ${:%d} (${1})") == "Ana: 7 (Ana)");
}

TEST_CASE("string template converter only takes string input",
          "[string_template]")
{
    DataConverterStringTemplate converter;
    converter.addArgument(numberArgument("a", 1.0f));
    DataValueNumber number(3.0f);
    auto output = converter.convert(&number, nullptr);
    REQUIRE(output->is<DataValueString>());
    CHECK(output->as<DataValueString>()->value() == "");
    CHECK(convert(converter, "${a}") == "1");
}

TEST_CASE("string template converter follows its delimiters",
          "[string_template]")
{
    DataConverterStringTemplate converter;
    CHECK(converter.openDelimiter() == "${");
    CHECK(converter.closeDelimiter() == "}");
    converter.addArgument(numberArgument("a", 1.0f));
    CHECK(convert(converter, "{a} ${a}") == "{a} 1");

    converter.openDelimiter("{");
    CHECK(convert(converter, "{a} ${a}") == "1 $1");

    converter.openDelimiter("%");
    converter.closeDelimiter("%");
    CHECK(convert(converter, "{a} ${a}") == "{a} ${a}");
    CHECK(convert(converter, "%a% %0:.1f%") == "1 1.0");

    // An invalid delimiter leaves the text as is.
    converter.closeDelimiter("");
    CHECK(convert(converter, "%a% %0:.1f%") == "%a% %0:.1f%");
}

TEST_CASE("string template converter uses the first argument with a name",
          "[string_template]")
{
    DataConverterStringTemplate converter;
    converter.addArgument(numberArgument("a", 1.0f));
    converter.addArgument(numberArgument("a", 2.0f));
    CHECK(convert(converter, "${a}") == "1");
}

TEST_CASE("string template clone retargets argument binds", "[string_template]")
{
    auto converter = new DataConverterStringTemplate();
    converter->openDelimiter("<<");
    converter->closeDelimiter(">>");
    converter->addArgument(numberArgument("a", 1.0f));
    converter->addArgument(stringArgument("b", "x"));

    auto converterBind = new DataBindContext();
    converterBind->target(converter);
    converterBind->propertyKey(
        DataConverterStringTemplateBase::openDelimiterPropertyKey);
    converter->addDataBind(converterBind);
    auto argumentBind = new DataBindContext();
    argumentBind->target(converter->arguments()[1]);
    argumentBind->propertyKey(
        StringTemplateArgumentStringBase::propertyValuePropertyKey);
    converter->addDataBind(argumentBind);

    auto cloned = converter->clone()->as<DataConverterStringTemplate>();
    REQUIRE(cloned->arguments().size() == 2);
    CHECK(cloned->arguments()[0] != converter->arguments()[0]);
    CHECK(cloned->arguments()[0]->name() == "a");
    CHECK(cloned->openDelimiter() == "<<");
    CHECK(cloned->closeDelimiter() == ">>");
    CHECK(cloned->arguments()[1]->name() == "b");
    REQUIRE(cloned->dataBinds().size() == 2);
    CHECK(cloned->dataBinds()[0]->target() == cloned);
    CHECK(cloned->dataBinds()[1]->target() == cloned->arguments()[1]);
    CHECK(convert(*cloned, "<<a>> <<b>>") == "1 x");

    delete cloned;
    delete converter;
}

TEST_CASE("string template and its arguments import", "[string_template]")
{
    RivBytes riv;
    riv.object(BackboardBase::typeKey);
    riv.end();
    riv.object(DataConverterStringTemplateBase::typeKey);
    riv.end();
    riv.object(StringTemplateArgumentNumberBase::typeKey);
    riv.propString(StringTemplateArgumentBase::namePropertyKey, "score");
    riv.propFloat(StringTemplateArgumentNumberBase::propertyValuePropertyKey,
                  7.25f);
    riv.end();
    // A bind on the argument lands in the template's bind list.
    riv.object(DataBindBase::typeKey);
    riv.propUint(DataBindBase::propertyKeyPropertyKey,
                 StringTemplateArgumentNumberBase::propertyValuePropertyKey);
    riv.end();
    riv.object(StringTemplateArgumentStringBase::typeKey);
    riv.propString(StringTemplateArgumentBase::namePropertyKey, "player");
    riv.propString(StringTemplateArgumentStringBase::propertyValuePropertyKey,
                   "Ana");
    riv.end();
    writeArtboardObject(riv);

    ImportResult result;
    auto file = File::import(riv.bytes(), &gNoOpFactory, &result);
    REQUIRE(result == ImportResult::success);
    REQUIRE(file != nullptr);
    auto& converters = file->testing_dataConverters();
    REQUIRE(converters.size() == 1);
    REQUIRE(converters[0]->is<DataConverterStringTemplate>());
    auto converter = converters[0]->as<DataConverterStringTemplate>();
    REQUIRE(converter->arguments().size() == 2);
    REQUIRE(converter->dataBinds().size() == 1);
    CHECK(converter->dataBinds()[0]->target() == converter->arguments()[0]);
    CHECK(convert(*converter, "${player} got ${score:%.1f}") == "Ana got 7.2");
}

TEST_CASE("string template delimiters import", "[string_template]")
{
    RivBytes riv;
    riv.object(BackboardBase::typeKey);
    riv.end();
    riv.object(DataConverterStringTemplateBase::typeKey);
    riv.propString(DataConverterStringTemplateBase::openDelimiterPropertyKey,
                   "%");
    riv.propString(DataConverterStringTemplateBase::closeDelimiterPropertyKey,
                   "%");
    riv.end();
    riv.object(StringTemplateArgumentNumberBase::typeKey);
    riv.propString(StringTemplateArgumentBase::namePropertyKey, "score");
    riv.propFloat(StringTemplateArgumentNumberBase::propertyValuePropertyKey,
                  7.5f);
    riv.end();
    writeArtboardObject(riv);

    ImportResult result;
    auto file = File::import(riv.bytes(), &gNoOpFactory, &result);
    REQUIRE(result == ImportResult::success);
    REQUIRE(file != nullptr);
    REQUIRE(file->testing_dataConverters().size() == 1);
    auto converter =
        file->testing_dataConverters()[0]->as<DataConverterStringTemplate>();
    CHECK(convert(*converter, "%score:05.1f% and ${score}") ==
          "007.5 and ${score}");
}

// Arguments find their template as the last imported converter. One that
// follows another kind of converter, or no converter at all, fails to import
// and is discarded without the rest of the file being affected (run under
// `./test.sh asan` to catch a dangling registration).
TEST_CASE("string template arguments without a template are skipped",
          "[string_template][malformed]")
{
    RivBytes riv;
    riv.object(BackboardBase::typeKey);
    riv.end();
    riv.object(StringTemplateArgumentNumberBase::typeKey);
    riv.propString(StringTemplateArgumentBase::namePropertyKey, "a");
    riv.end();
    riv.object(DataConverterRounderBase::typeKey);
    riv.end();
    riv.object(StringTemplateArgumentStringBase::typeKey);
    riv.propString(StringTemplateArgumentBase::namePropertyKey, "b");
    riv.end();
    writeArtboardObject(riv);

    ImportResult result;
    auto file = File::import(riv.bytes(), &gNoOpFactory, &result);
    REQUIRE(result == ImportResult::success);
    REQUIRE(file != nullptr);
    REQUIRE(file->artboard() != nullptr);
    CHECK(file->artboard()->name() == "A");
    REQUIRE(file->testing_dataConverters().size() == 1);
    CHECK(file->testing_dataConverters()[0]->is<DataConverterRounder>());
}

// Asset generated by rive_core/test/data_converter_string_template_test.dart:
// a custom string property "result" bound to the view model "VM" string
// "message" ("{player} got {0:%d} right") through a string template with
// delimiters "{" and "}". Its arguments are, in order, bound to the number
// "numerical_score" (7) and the string "player" ("Ana").
TEST_CASE("string template arguments follow their view model properties",
          "[string_template]")
{
    auto file = ReadRiveFile("assets/string_template.riv");
    auto artboard = file->artboard()->instance();
    REQUIRE(artboard != nullptr);

    auto viewModel = file->viewModelByName("VM");
    REQUIRE(viewModel != nullptr);
    auto viewModelInstance = viewModel->createDefaultInstance();
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance->instance());
    artboard->advance(0.0f);

    auto result = artboard->find<CustomPropertyString>("result");
    REQUIRE(result != nullptr);
    CHECK(result->propertyValue() == "Ana got 7 right");

    auto score = viewModelInstance->propertyNumber("numerical_score");
    auto player = viewModelInstance->propertyString("player");
    REQUIRE(score != nullptr);
    REQUIRE(player != nullptr);
    score->value(8.6f);
    artboard->advance(0.0f);
    CHECK(result->propertyValue() == "Ana got 9 right");

    player->value("Bo");
    artboard->advance(0.0f);
    CHECK(result->propertyValue() == "Bo got 9 right");

    // The template comes from the view model too.
    auto message = viewModelInstance->propertyString("message");
    REQUIRE(message != nullptr);
    message->value("{1} has {} points");
    artboard->advance(0.0f);
    CHECK(result->propertyValue() == "Bo has 8.6 points");
}

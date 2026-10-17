#include "rive/data_bind/converters/string_template/string_template_argument_number.hpp"
#include "rive/data_bind/converters/string_template/string_template_format.hpp"

using namespace rive;

void StringTemplateArgumentNumber::appendValue(
    std::string& output,
    const std::string& spec) const
{
  output += StringTemplateFormat::formatNumber(propertyValue(), spec);
}

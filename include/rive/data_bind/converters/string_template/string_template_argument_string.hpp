#ifndef _RIVE_STRING_TEMPLATE_ARGUMENT_STRING_HPP_
#define _RIVE_STRING_TEMPLATE_ARGUMENT_STRING_HPP_
#include "rive/generated/data_bind/converters/string_template/string_template_argument_string_base.hpp"
#include <stdio.h>
namespace rive
{
class StringTemplateArgumentString : public StringTemplateArgumentStringBase
{
public:
  // Strings are inserted as is, the spec only applies to numbers.
  void appendValue(std::string& output, const std::string& spec) const override
  {
    output += propertyValue();
  }
};
} // namespace rive

#endif

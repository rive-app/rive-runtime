#ifndef _RIVE_STRING_TEMPLATE_ARGUMENT_NUMBER_HPP_
#define _RIVE_STRING_TEMPLATE_ARGUMENT_NUMBER_HPP_
#include "rive/generated/data_bind/converters/string_template/string_template_argument_number_base.hpp"
#include <stdio.h>
namespace rive
{
class StringTemplateArgumentNumber : public StringTemplateArgumentNumberBase
{
public:
  void appendValue(std::string& output, const std::string& spec) const override;
};
} // namespace rive

#endif

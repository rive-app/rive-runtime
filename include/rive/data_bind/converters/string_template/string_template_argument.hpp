#ifndef _RIVE_STRING_TEMPLATE_ARGUMENT_HPP_
#define _RIVE_STRING_TEMPLATE_ARGUMENT_HPP_
#include "rive/generated/data_bind/converters/string_template/string_template_argument_base.hpp"
#include "rive/data_bind/data_bind.hpp"
#include <stdio.h>
#include <string>
namespace rive
{
class DataConverterStringTemplate;
class StringTemplateArgument : public StringTemplateArgumentBase
{
public:
  StatusCode import(ImportStack& importStack) override;
  void addDataBind(DataBind* dataBind);
  void stringTemplate(DataConverterStringTemplate* value)
  {
    m_stringTemplate = value;
  }
  // Appends this argument's value to output, formatted with the
  // placeholder's spec.
  virtual void appendValue(std::string& output, const std::string& spec)
      const = 0;

private:
  DataConverterStringTemplate* m_stringTemplate = nullptr;
};
} // namespace rive

#endif

#ifndef _RIVE_DATA_CONVERTER_STRING_TEMPLATE_HPP_
#define _RIVE_DATA_CONVERTER_STRING_TEMPLATE_HPP_
#include "rive/generated/data_bind/converters/data_converter_string_template_base.hpp"
#include "rive/data_bind/converters/string_template/string_template_argument.hpp"
#include "rive/data_bind/converters/string_template/string_template_format.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include <stdio.h>
#include <vector>
namespace rive
{
// Uses its string input as a template and replaces the placeholders with
// argument values: ${name} by name, ${0} by position and ${} with the next
// argument. Placeholders without an argument become empty text. The delimiters
// are configurable.
class DataConverterStringTemplate : public DataConverterStringTemplateBase
{
public:
  ~DataConverterStringTemplate();
  DataValue* convert(DataValue* value, DataBind* dataBind) override;
  DataType outputType() override { return DataType::string; };
  void addArgument(StringTemplateArgument* argument);
  const std::vector<StringTemplateArgument*>& arguments() const
  {
    return m_arguments;
  }
  Core* clone() const override;

protected:
  void openDelimiterChanged() override;
  void closeDelimiterChanged() override;

private:
  StringTemplateArgument* argument(const std::string& name) const;
  std::vector<StringTemplateArgument*> m_arguments;
  std::vector<StringTemplateSegment> m_segments;
  // The input m_segments was parsed from.
  std::string m_parsedText;
  // Deserialize and copy don't call the changed hooks, so start dirty.
  bool m_segmentsDirty = true;
  DataValueString m_output;
};
} // namespace rive

#endif

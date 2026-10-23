#include "rive/generated/data_bind/converters/data_converter_string_template_base.hpp"
#include "rive/data_bind/converters/data_converter_string_template.hpp"

using namespace rive;

Core* DataConverterStringTemplateBase::clone() const
{
  auto cloned = new DataConverterStringTemplate();
  cloned->copy(*this);
  return cloned;
}

#include "rive/generated/data_bind/converters/string_template/string_template_argument_string_base.hpp"
#include "rive/data_bind/converters/string_template/string_template_argument_string.hpp"

using namespace rive;

Core* StringTemplateArgumentStringBase::clone() const
{
  auto cloned = new StringTemplateArgumentString();
  cloned->copy(*this);
  return cloned;
}

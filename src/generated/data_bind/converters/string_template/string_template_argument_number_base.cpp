#include "rive/generated/data_bind/converters/string_template/string_template_argument_number_base.hpp"
#include "rive/data_bind/converters/string_template/string_template_argument_number.hpp"

using namespace rive;

Core* StringTemplateArgumentNumberBase::clone() const
{
  auto cloned = new StringTemplateArgumentNumber();
  cloned->copy(*this);
  return cloned;
}

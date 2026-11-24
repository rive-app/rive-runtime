#include "rive/generated/text/text_style_decoration_base.hpp"
#include "rive/text/text_style_decoration.hpp"

using namespace rive;

Core* TextStyleDecorationBase::clone() const
{
  auto cloned = new TextStyleDecoration();
  cloned->copy(*this);
  return cloned;
}

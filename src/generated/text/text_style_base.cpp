#include "rive/generated/text/text_style_base.hpp"
#include "rive/text/text_style.hpp"

using namespace rive;

Core* TextStyleBase::clone() const
{
    auto cloned = new TextStyle();
    cloned->copy(*this);
    return cloned;
}

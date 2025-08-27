#include "rive/generated/custom_property_color_base.hpp"
#include "rive/custom_property_color.hpp"

using namespace rive;

Core* CustomPropertyColorBase::clone() const
{
    auto cloned = new CustomPropertyColor();
    cloned->copy(*this);
    return cloned;
}

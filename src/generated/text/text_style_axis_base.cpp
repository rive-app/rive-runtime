#include "rive/generated/text/text_style_axis_base.hpp"
#include "rive/text/text_style_axis.hpp"

using namespace rive;

Core* TextStyleAxisBase::clone() const
{
    auto cloned = new TextStyleAxis();
    cloned->copy(*this);
    return cloned;
}

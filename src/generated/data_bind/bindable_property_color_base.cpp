#include "rive/generated/data_bind/bindable_property_color_base.hpp"
#include "rive/data_bind/bindable_property_color.hpp"

using namespace rive;

Core* BindablePropertyColorBase::clone() const
{
    auto cloned = new BindablePropertyColor();
    cloned->copy(*this);
    return cloned;
}

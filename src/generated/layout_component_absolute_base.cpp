#include "rive/generated/layout_component_absolute_base.hpp"
#include "rive/layout_component_absolute.hpp"

using namespace rive;

Core* AbsoluteLayoutComponentBase::clone() const
{
    auto cloned = new AbsoluteLayoutComponent();
    cloned->copy(*this);
    return cloned;
}

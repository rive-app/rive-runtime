#include "rive/generated/layout/layout_component_style_base.hpp"
#include "rive/layout/layout_component_style.hpp"

using namespace rive;

Core* LayoutComponentStyleBase::clone() const
{
    auto cloned = new LayoutComponentStyle();
    cloned->copy(*this);
    return cloned;
}

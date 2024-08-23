#include "rive/generated/layout_component_base.hpp"
#include "rive/layout_component.hpp"

using namespace rive;

Core* LayoutComponentBase::clone() const
{
    auto cloned = new LayoutComponent();
    cloned->copy(*this);
    return cloned;
}

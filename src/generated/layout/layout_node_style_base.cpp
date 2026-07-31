#include "rive/generated/layout/layout_node_style_base.hpp"
#include "rive/layout/layout_node_style.hpp"

using namespace rive;

Core* LayoutNodeStyleBase::clone() const
{
    auto cloned = new LayoutNodeStyle();
    cloned->copy(*this);
    return cloned;
}

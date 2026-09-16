#include "rive/generated/selection_style_base.hpp"
#include "rive/selection_style.hpp"

using namespace rive;

Core* SelectionStyleBase::clone() const
{
    auto cloned = new SelectionStyle();
    cloned->copy(*this);
    return cloned;
}

#include "rive/generated/constraints/scroll_bar_constraint_base.hpp"
#include "rive/constraints/scroll_bar_constraint.hpp"

using namespace rive;

Core* ScrollBarConstraintBase::clone() const
{
    auto cloned = new ScrollBarConstraint();
    cloned->copy(*this);
    return cloned;
}

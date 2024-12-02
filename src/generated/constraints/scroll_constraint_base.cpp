#include "rive/generated/constraints/scroll_constraint_base.hpp"
#include "rive/constraints/scroll_constraint.hpp"

using namespace rive;

Core* ScrollConstraintBase::clone() const
{
    auto cloned = new ScrollConstraint();
    cloned->copy(*this);
    return cloned;
}

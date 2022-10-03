#include "rive/generated/constraints/scale_constraint_base.hpp"
#include "rive/constraints/scale_constraint.hpp"

using namespace rive;

Core* ScaleConstraintBase::clone() const
{
    auto cloned = new ScaleConstraint();
    cloned->copy(*this);
    return cloned;
}

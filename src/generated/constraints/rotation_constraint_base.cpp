#include "rive/generated/constraints/rotation_constraint_base.hpp"
#include "rive/constraints/rotation_constraint.hpp"

using namespace rive;

Core* RotationConstraintBase::clone() const
{
    auto cloned = new RotationConstraint();
    cloned->copy(*this);
    return cloned;
}

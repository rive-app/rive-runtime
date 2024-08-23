#include "rive/generated/constraints/ik_constraint_base.hpp"
#include "rive/constraints/ik_constraint.hpp"

using namespace rive;

Core* IKConstraintBase::clone() const
{
    auto cloned = new IKConstraint();
    cloned->copy(*this);
    return cloned;
}

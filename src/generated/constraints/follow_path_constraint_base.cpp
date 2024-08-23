#include "rive/generated/constraints/follow_path_constraint_base.hpp"
#include "rive/constraints/follow_path_constraint.hpp"

using namespace rive;

Core* FollowPathConstraintBase::clone() const
{
    auto cloned = new FollowPathConstraint();
    cloned->copy(*this);
    return cloned;
}

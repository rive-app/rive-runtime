#include "rive/generated/constraints/list_follow_path_constraint_base.hpp"
#include "rive/constraints/list_follow_path_constraint.hpp"

using namespace rive;

Core* ListFollowPathConstraintBase::clone() const
{
    auto cloned = new ListFollowPathConstraint();
    cloned->copy(*this);
    return cloned;
}

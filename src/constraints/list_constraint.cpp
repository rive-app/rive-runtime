#include "rive/component.hpp"
#include "rive/constraints/follow_path_constraint.hpp"
#include "rive/constraints/list_constraint.hpp"

using namespace rive;

ListConstraint* ListConstraint::from(Component* component)
{
    switch (component->coreType())
    {
        case FollowPathConstraintBase::typeKey:
            return component->as<FollowPathConstraint>();
    }
    return nullptr;
}
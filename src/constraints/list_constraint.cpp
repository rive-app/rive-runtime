#include "rive/component.hpp"
#include "rive/constraints/list_follow_path_constraint.hpp"
#include "rive/constraints/list_constraint.hpp"

using namespace rive;

ListConstraint* ListConstraint::from(Component* component)
{
    switch (component->coreType())
    {
        case ListFollowPathConstraintBase::typeKey:
            return component->as<ListFollowPathConstraint>();
    }
    return nullptr;
}
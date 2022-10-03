#include "rive/constraints/constraint.hpp"
#include "rive/container_component.hpp"
#include "rive/transform_component.hpp"
#include "rive/core_context.hpp"
#include "rive/math/mat2d.hpp"

using namespace rive;

StatusCode Constraint::onAddedClean(CoreContext* context)
{
    if (!parent()->is<TransformComponent>())
    {
        return StatusCode::InvalidObject;
    }

    parent()->as<TransformComponent>()->addConstraint(this);

    return StatusCode::Ok;
}

void Constraint::markConstraintDirty() { parent()->as<TransformComponent>()->markTransformDirty(); }

void Constraint::strengthChanged() { markConstraintDirty(); }

void Constraint::buildDependencies()
{
    Super::buildDependencies();
    parent()->addDependent(this);
}

void Constraint::onDirty(ComponentDirt dirt)
{
    // Whenever the constraint gets any dirt, make sure to mark the constrained
    // component dirty.
    markConstraintDirty();
}

static Mat2D identity;
const Mat2D& rive::getParentWorld(const TransformComponent& component)
{
    auto parent = component.parent();
    if (parent->is<WorldTransformComponent>())
    {
        return parent->as<WorldTransformComponent>()->worldTransform();
    }
    return identity;
}
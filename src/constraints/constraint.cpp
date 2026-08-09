#include "rive/constraints/constraint.hpp"
#include "rive/container_component.hpp"
#include "rive/transform_component.hpp"
#include "rive/core_context.hpp"
#include "rive/math/mat2d.hpp"

using namespace rive;

StatusCode Constraint::onAddedDirty(CoreContext* context)
{
    StatusCode result = Super::onAddedDirty(context);
    if (!parent()->is<TransformComponent>())
    {
        return StatusCode::InvalidObject;
    }

    parent()->as<TransformComponent>()->addConstraint(this);

    return result;
}

void Constraint::markConstraintDirty()
{
    parent()->as<TransformComponent>()->markTransformDirty();
}

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

void Constraint::composeKeepingAnchor(TransformComponent* component,
                                      const TransformComponents& composed)
{
    Mat2D& world = component->mutableWorldTransform();
    const Vec2D anchor = component->localAnchor();
    if (anchor.x == 0.0f && anchor.y == 0.0f)
    {
        world = Mat2D::compose(composed);
        return;
    }
    const Vec2D before = world * anchor;
    Mat2D result = Mat2D::compose(composed);
    const Vec2D after = result * anchor;
    result[4] += before.x - after.x;
    result[5] += before.y - after.y;
    world = result;
}

void Constraint::landAnchor(TransformComponent* component, float strength)
{
    const Vec2D anchor = component->localAnchor();
    if (anchor.x == 0.0f && anchor.y == 0.0f)
    {
        return;
    }
    Mat2D& world = component->mutableWorldTransform();
    world[4] -= (world[0] * anchor.x + world[2] * anchor.y) * strength;
    world[5] -= (world[1] * anchor.x + world[3] * anchor.y) * strength;
}

void Constraint::composeLandingAnchor(TransformComponent* component,
                                      const TransformComponents& composed,
                                      float strength)
{
    component->mutableWorldTransform() = Mat2D::compose(composed);
    landAnchor(component, strength);
}

Mat2D Constraint::offsetInParentFrame(TransformComponent* component,
                                      const Mat2D& offset)
{
    const Mat2D& parentWorld = getParentWorld(*component);
    const Vec2D delta =
        (parentWorld * offset).translation() - parentWorld.translation();
    Mat2D result = component->worldTransform();
    result[4] += delta.x;
    result[5] += delta.y;
    return result;
}

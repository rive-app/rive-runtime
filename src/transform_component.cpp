#include "rive/transform_component.hpp"
#include "rive/world_transform_component.hpp"
#include "rive/shapes/clipping_shape.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/constraints/constraint.hpp"

using namespace rive;

StatusCode TransformComponent::onAddedClean(CoreContext* context)
{
    m_ParentTransformComponent = parent() != nullptr && parent()->is<WorldTransformComponent>()
                                     ? parent()->as<WorldTransformComponent>()
                                     : nullptr;
    return StatusCode::Ok;
}

bool TransformComponent::collapse(bool value)
{
    if (!Super::collapse(value))
    {
        return false;
    }
    for (auto d : dependents())
    {
        if (d->is<TransformComponent>())
        {
            auto dependent = d->as<TransformComponent>();
            dependent->markDirtyIfConstrained();
        }
    }
    return true;
}

// If the component has any constraints applied we mark it as dirty
// because one of its constraining targets has changed its collapse
// status.
void TransformComponent::markDirtyIfConstrained()
{
    if (m_Constraints.size() > 0)
    {
        addDirt(ComponentDirt::WorldTransform, true);
    }
}

void TransformComponent::buildDependencies()
{
    if (parent() != nullptr)
    {
        parent()->addDependent(this);
    }
}

void TransformComponent::markTransformDirty()
{
    if (!addDirt(ComponentDirt::Transform))
    {
        return;
    }
    markWorldTransformDirty();
}

void TransformComponent::updateTransform()
{
    m_Transform = Mat2D::fromRotation(rotation());
    m_Transform[4] = x();
    m_Transform[5] = y();
    m_Transform.scaleByValues(scaleX(), scaleY());
}

AABB TransformComponent::localBounds() const { return AABB(); }

void TransformComponent::updateWorldTransform()
{
    if (m_ParentTransformComponent != nullptr)
    {
        m_WorldTransform = m_ParentTransformComponent->m_WorldTransform * m_Transform;
    }
    else
    {
        m_WorldTransform = m_Transform;
    }
    updateConstraints();
}

void TransformComponent::updateConstraints()
{
    for (auto constraint : m_Constraints)
    {
        constraint->constrain(this);
    }
}

void TransformComponent::update(ComponentDirt value)
{
    if (hasDirt(value, ComponentDirt::Transform))
    {
        updateTransform();
    }
    if (hasDirt(value, ComponentDirt::WorldTransform))
    {
        updateWorldTransform();
    }
    if (hasDirt(value, ComponentDirt::RenderOpacity))
    {
        m_RenderOpacity = opacity();
        if (m_ParentTransformComponent != nullptr)
        {
            m_RenderOpacity *= m_ParentTransformComponent->childOpacity();
        }
    }
}

const Mat2D& TransformComponent::transform() const { return m_Transform; }

Mat2D& TransformComponent::mutableTransform() { return m_Transform; }

void TransformComponent::rotationChanged() { markTransformDirty(); }
void TransformComponent::scaleXChanged() { markTransformDirty(); }
void TransformComponent::scaleYChanged() { markTransformDirty(); }

void TransformComponent::addConstraint(Constraint* constraint)
{
    m_Constraints.push_back(constraint);
}

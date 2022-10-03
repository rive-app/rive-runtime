#include "rive/transform_component.hpp"
#include "rive/shapes/clipping_shape.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/constraints/constraint.hpp"

using namespace rive;

float WorldTransformComponent::childOpacity() { return opacity(); }

void WorldTransformComponent::markWorldTransformDirty()
{
    addDirt(ComponentDirt::WorldTransform, true);
}

const Mat2D& WorldTransformComponent::worldTransform() const { return m_WorldTransform; }

Mat2D& WorldTransformComponent::mutableWorldTransform() { return m_WorldTransform; }

void WorldTransformComponent::opacityChanged() { addDirt(ComponentDirt::RenderOpacity, true); }

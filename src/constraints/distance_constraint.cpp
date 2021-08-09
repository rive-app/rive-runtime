#include "rive/constraints/distance_constraint.hpp"
#include "rive/bones/bone.hpp"
#include "rive/artboard.hpp"
#include <algorithm>

using namespace rive;

enum class Mode
{
	Closer = 0,
	Further = 1,
	Exact = 2
};

void DistanceConstraint::constrain(TransformComponent* component)
{
	if (m_Target == nullptr)
	{
		return;
	}

	Vec2D targetTranslation;
	m_Target->worldTranslation(targetTranslation);

	Vec2D ourTranslation;
	component->worldTranslation(ourTranslation);

	Vec2D toTarget;
	Vec2D::subtract(toTarget, ourTranslation, targetTranslation);
	float currentDistance = Vec2D::length(toTarget);
	switch (static_cast<Mode>(modeValue()))
	{
		case Mode::Closer:
			if (currentDistance < distance())
			{
				return;
			}
			break;
		case Mode::Further:
			if (currentDistance > distance())
			{
				return;
			}
			break;
		default:
			break;
	}
	if (currentDistance < 0.001f)
	{
		return;
	}

	Vec2D::scale(toTarget, toTarget, 1.0f / currentDistance);
	Vec2D::scale(toTarget, toTarget, distance());

	Mat2D& world = component->mutableWorldTransform();
	Vec2D position;
	Vec2D::add(position, targetTranslation, toTarget);
	Vec2D::lerp(position, ourTranslation, position, strength());
	world[4] = position[0];
	world[5] = position[1];
}
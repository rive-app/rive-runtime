#include "rive/constraints/transform_constraint.hpp"
#include "rive/transform_component.hpp"
#include "rive/math/mat2d.hpp"
#include <cmath>

using namespace rive;

void TransformConstraint::constrain(TransformComponent* component)
{
	if (m_Target == nullptr)
	{
		return;
	}

	const Mat2D& transformA = component->worldTransform();
	Mat2D transformB(m_Target->worldTransform());
	if (sourceSpace() == TransformSpace::local)
	{
		const Mat2D& targetParentWorld = getParentWorld(*m_Target);

		Mat2D inverse;
		if (!Mat2D::invert(inverse, targetParentWorld))
		{
			return;
		}
		Mat2D::multiply(transformB, inverse, transformB);
	}
	if (destSpace() == TransformSpace::local)
	{
		const Mat2D& targetParentWorld = getParentWorld(*component);
		Mat2D::multiply(transformB, targetParentWorld, transformB);
	}

	Mat2D::decompose(m_ComponentsA, transformA);
	Mat2D::decompose(m_ComponentsB, transformB);

	float angleA = std::fmod(m_ComponentsA.rotation(), (float)M_PI * 2);
	float angleB = std::fmod(m_ComponentsB.rotation(), (float)M_PI * 2);
	float diff = angleB - angleA;
	if (diff > M_PI)
	{
		diff -= M_PI * 2;
	}
	else if (diff < -M_PI)
	{
		diff += M_PI * 2;
	}

	float t = strength();
	float ti = 1.0f - t;

	m_ComponentsB.rotation(angleA + diff * t);
	m_ComponentsB.x(m_ComponentsA.x() * ti + m_ComponentsB.x() * t);
	m_ComponentsB.y(m_ComponentsA.y() * ti + m_ComponentsB.y() * t);
	m_ComponentsB.scaleX(m_ComponentsA.scaleX() * ti +
	                     m_ComponentsB.scaleX() * t);
	m_ComponentsB.scaleY(m_ComponentsA.scaleY() * ti +
	                     m_ComponentsB.scaleY() * t);
	m_ComponentsB.skew(m_ComponentsA.skew() * ti + m_ComponentsB.skew() * t);

	Mat2D::compose(component->mutableWorldTransform(), m_ComponentsB);
}

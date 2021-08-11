#include "rive/constraints/scale_constraint.hpp"
#include "rive/transform_component.hpp"
#include "rive/math/mat2d.hpp"
#include <cmath>

using namespace rive;

void ScaleConstraint::constrain(TransformComponent* component)
{
	const Mat2D& transformA = component->worldTransform();
	Mat2D transformB;
	Mat2D::decompose(m_ComponentsA, transformA);
	if (m_Target == nullptr)
	{
		Mat2D::copy(transformB, transformA);
		TransformComponents::copy(m_ComponentsB, m_ComponentsA);
	}
	else
	{
		Mat2D::copy(transformB, m_Target->worldTransform());
		if (sourceSpace() == TransformSpace::local)
		{
			Mat2D inverse;
			if (!Mat2D::invert(inverse, getParentWorld(*m_Target)))
			{
				return;
			}
			Mat2D::multiply(transformB, inverse, transformB);
		}
		Mat2D::decompose(m_ComponentsB, transformB);

		if (!doesCopy())
		{
			m_ComponentsB.scaleX(destSpace() == TransformSpace::local
			                         ? 1.0f
			                         : m_ComponentsA.scaleX());
		}
		else
		{
			m_ComponentsB.scaleX(m_ComponentsB.scaleX() * copyFactor());
			if (offset())
			{
				m_ComponentsB.scaleX(m_ComponentsB.scaleX() *
				                     component->scaleX());
			}
		}

		if (!doesCopyY())
		{
			m_ComponentsB.scaleY(destSpace() == TransformSpace::local
			                         ? 1.0f
			                         : m_ComponentsA.scaleY());
		}
		else
		{
			m_ComponentsB.scaleY(m_ComponentsB.scaleY() * copyFactorY());
			if (offset())
			{
				m_ComponentsB.scaleY(m_ComponentsB.scaleY() *
				                     component->scaleY());
			}
		}

		if (destSpace() == TransformSpace::local)
		{
			// Destination space is in parent transform coordinates. Recompose
			// the parent local transform and get it in world, then decompose
			// the world for interpolation.

			Mat2D::compose(transformB, m_ComponentsB);
			Mat2D::multiply(transformB, getParentWorld(*component), transformB);
			Mat2D::decompose(m_ComponentsB, transformB);
		}
	}

	bool clamplocal = minMaxSpace() == TransformSpace::local;
	if (clamplocal)
	{
		// Apply min max in local space, so transform to local coordinates
		// first.
		Mat2D::compose(transformB, m_ComponentsB);
		Mat2D inverse;
		if (!Mat2D::invert(inverse, getParentWorld(*component)))
		{
			return;
		}
		Mat2D::multiply(transformB, inverse, transformB);
		Mat2D::decompose(m_ComponentsB, transformB);
	}
	if (max() && m_ComponentsB.scaleX() > maxValue())
	{
		m_ComponentsB.scaleX(maxValue());
	}
	if (min() && m_ComponentsB.scaleX() < minValue())
	{
		m_ComponentsB.scaleX(minValue());
	}
	if (maxY() && m_ComponentsB.scaleY() > maxValueY())
	{
		m_ComponentsB.scaleY(maxValueY());
	}
	if (minY() && m_ComponentsB.scaleY() < minValueY())
	{
		m_ComponentsB.scaleY(minValueY());
	}
	if (clamplocal)
	{
		// Transform back to world.
		Mat2D::compose(transformB, m_ComponentsB);
		Mat2D::multiply(transformB, getParentWorld(*component), transformB);
		Mat2D::decompose(m_ComponentsB, transformB);
	}

	float t = strength();
	float ti = 1.0f - t;

	m_ComponentsB.rotation(m_ComponentsA.rotation());
	m_ComponentsB.x(m_ComponentsA.x());
	m_ComponentsB.y(m_ComponentsA.y());
	m_ComponentsB.scaleX(m_ComponentsA.scaleX() * ti +
	                     m_ComponentsB.scaleX() * t);
	m_ComponentsB.scaleY(m_ComponentsA.scaleY() * ti +
	                     m_ComponentsB.scaleY() * t);
	m_ComponentsB.skew(m_ComponentsA.skew());

	Mat2D::compose(component->mutableWorldTransform(), m_ComponentsB);
}

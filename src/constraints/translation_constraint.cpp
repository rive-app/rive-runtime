#include "rive/constraints/translation_constraint.hpp"
#include "rive/transform_component.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/math/vec2d.hpp"
#include <cmath>

using namespace rive;

void TranslationConstraint::constrain(TransformComponent* component)
{
	Mat2D& transformA = component->mutableWorldTransform();
	Vec2D translationA(transformA[4], transformA[5]);
	Vec2D translationB;
	if (m_Target == nullptr)
	{
		Vec2D::copy(translationB, translationA);
	}
	else
	{
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
		translationB[0] = transformB[4];
		translationB[1] = transformB[5];

		if (!doesCopy())
		{
			translationB[0] =
			    destSpace() == TransformSpace::local ? 0.0f : translationA[0];
		}
		else
		{
			translationB[0] *= copyFactor();
			if (offset())
			{
				translationB[0] += component->x();
			}
		}

		if (!doesCopyY())
		{
			translationB[1] =
			    destSpace() == TransformSpace::local ? 0.0f : translationA[1];
		}
		else
		{
			translationB[1] *= copyFactorY();

			if (offset())
			{
				translationB[1] += component->y();
			}
		}

		if (destSpace() == TransformSpace::local)
		{
			// Destination space is in parent transform coordinates.
			Vec2D::transform(
			    translationB, translationB, getParentWorld(*component));
		}
	}

	bool clampLocal = minMaxSpace() == TransformSpace::local;
	if (clampLocal)
	{
		// Apply min max in local space, so transform to local coordinates
		// first.
		Mat2D invert;
		if (!Mat2D::invert(invert, getParentWorld(*component)))
		{
			return;
		}
		// Get our target world coordinates in parent local.
		Vec2D::transform(translationB, translationB, invert);
	}
	if (max() && translationB[0] > maxValue())
	{
		translationB[0] = maxValue();
	}
	if (min() && translationB[0] < minValue())
	{
		translationB[0] = minValue();
	}
	if (maxY() && translationB[1] > maxValueY())
	{
		translationB[1] = maxValueY();
	}
	if (minY() && translationB[1] < minValueY())
	{
		translationB[1] = minValueY();
	}
	if (clampLocal)
	{
		// Transform back to world.
		Vec2D::transform(
		    translationB, translationB, getParentWorld(*component));
	}

	float t = strength();
	float ti = 1.0f - t;

	// Just interpolate world translation
	transformA[4] = translationA[0] * ti + translationB[0] * t;
	transformA[5] = translationA[1] * ti + translationB[1] * t;
}

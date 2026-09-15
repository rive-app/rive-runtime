#include "rive/constraints/translation_constraint.hpp"
#include "rive/transform_component.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/math/vec2d.hpp"
#include <cmath>

using namespace rive;

void TranslationConstraint::constrain(TransformComponent* component)
{
    auto* tgt = target();
    if (tgt != nullptr && tgt->isCollapsed())
    {
        return;
    }
    Mat2D& transformA = component->mutableWorldTransform();
    Vec2D translationA(transformA[4], transformA[5]);
    Vec2D translationB;
    if (tgt == nullptr)
    {
        translationB = translationA;
    }
    else
    {
        Vec2D localA = offset() && (doesCopy() || doesCopyY())
                           ? component->composedTranslation()
                           : Vec2D();
        Mat2D transformB(tgt->worldTransform());
        if (sourceSpace() == TransformSpace::local)
        {
            const Mat2D& targetParentWorld = getParentWorld(*tgt);

            Mat2D inverse;
            if (!targetParentWorld.invert(&inverse))
            {
                return;
            }
            transformB = inverse * transformB;
        }
        translationB = transformB.translation();

        if (!doesCopy())
        {
            translationB.x =
                destSpace() == TransformSpace::local ? 0.0f : translationA.x;
        }
        else
        {
            translationB.x *= copyFactor();
            if (offset())
            {
                translationB.x += localA.x;
            }
        }

        if (!doesCopyY())
        {
            translationB.y =
                destSpace() == TransformSpace::local ? 0.0f : translationA.y;
        }
        else
        {
            translationB.y *= copyFactorY();

            if (offset())
            {
                translationB.y += localA.y;
            }
        }

        if (destSpace() == TransformSpace::local)
        {
            // Destination space is in parent transform coordinates.
            translationB = getParentWorld(*component) * translationB;
        }
    }

    bool clampLocal = minMaxSpace() == TransformSpace::local;
    if (clampLocal)
    {
        // Apply min max in local space, so transform to local coordinates
        // first.
        Mat2D inverse;
        if (!getParentWorld(*component).invert(&inverse))
        {
            return;
        }
        // Get our target world coordinates in parent local.
        translationB = inverse * translationB;
    }
    if (max() && translationB.x > maxValue())
    {
        translationB.x = maxValue();
    }
    if (min() && translationB.x < minValue())
    {
        translationB.x = minValue();
    }
    if (maxY() && translationB.y > maxValueY())
    {
        translationB.y = maxValueY();
    }
    if (minY() && translationB.y < minValueY())
    {
        translationB.y = minValueY();
    }
    if (clampLocal)
    {
        // Transform back to world.
        translationB = getParentWorld(*component) * translationB;
    }

    float t = strength();
    float ti = 1.0f - t;

    // Just interpolate world translation
    transformA[4] = translationA.x * ti + translationB.x * t;
    transformA[5] = translationA.y * ti + translationB.y * t;
    landAnchor(component, t);
}

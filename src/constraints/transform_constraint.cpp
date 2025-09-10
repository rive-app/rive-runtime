#include "rive/constraints/transform_constraint.hpp"
#include "rive/transform_component.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/math/math_types.hpp"
#include "rive/math/aabb.hpp"

using namespace rive;

const Mat2D TransformConstraint::targetTransform() const
{
    AABB bounds = m_Target->constraintBounds();
    Mat2D local =
        Mat2D::fromTranslate(bounds.left() + bounds.width() * originX(),
                             bounds.top() + bounds.height() * originY());
    return m_Target->worldTransform() * local;
}

void TransformConstraint::originXChanged() { markConstraintDirty(); }

void TransformConstraint::originYChanged() { markConstraintDirty(); }

void TransformConstraint::constrain(TransformComponent* component)
{
    if (m_Target == nullptr || m_Target->isCollapsed())
    {
        return;
    }

    const Mat2D& transformA = component->worldTransform();
    Mat2D transformB(targetTransform());
    if (sourceSpace() == TransformSpace::local)
    {
        const Mat2D& targetParentWorld = getParentWorld(*m_Target);

        Mat2D inverse;
        if (!targetParentWorld.invert(&inverse))
        {
            return;
        }
        transformB = inverse * transformB;
    }
    if (destSpace() == TransformSpace::local)
    {
        const Mat2D& targetParentWorld = getParentWorld(*component);
        transformB = targetParentWorld * transformB;
    }

    TransformConstraint::constrainWorld(component,
                                        transformA,
                                        m_ComponentsA,
                                        transformB,
                                        m_ComponentsB,
                                        strength());
}

void TransformConstraint::constrainWorld(TransformComponent* component,
                                         Mat2D from,
                                         TransformComponents componentsFrom,
                                         Mat2D to,
                                         TransformComponents componentsTo,
                                         float strength)
{
    componentsFrom = from.decompose();
    componentsTo = to.decompose();

    float angleA = std::fmod(componentsFrom.rotation(), math::PI * 2);
    float angleB = std::fmod(componentsTo.rotation(), math::PI * 2);
    float diff = angleB - angleA;
    if (diff > math::PI)
    {
        diff -= math::PI * 2;
    }
    else if (diff < -math::PI)
    {
        diff += math::PI * 2;
    }

    float t = strength;
    float ti = 1.0f - t;

    componentsTo.rotation(angleA + diff * t);
    componentsTo.x(componentsFrom.x() * ti + componentsTo.x() * t);
    componentsTo.y(componentsFrom.y() * ti + componentsTo.y() * t);
    componentsTo.scaleX(componentsFrom.scaleX() * ti +
                        componentsTo.scaleX() * t);
    componentsTo.scaleY(componentsFrom.scaleY() * ti +
                        componentsTo.scaleY() * t);
    componentsTo.skew(componentsFrom.skew() * ti + componentsTo.skew() * t);

    component->mutableWorldTransform() = Mat2D::compose(componentsTo);
}
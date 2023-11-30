#include "rive/constraints/scale_constraint.hpp"
#include "rive/transform_component.hpp"
#include "rive/math/mat2d.hpp"
#include <cmath>

using namespace rive;

void ScaleConstraint::constrain(TransformComponent* component)
{
    if (m_Target != nullptr && m_Target->isCollapsed())
    {
        return;
    }
    const Mat2D& transformA = component->worldTransform();
    Mat2D transformB;
    m_ComponentsA = transformA.decompose();
    if (m_Target == nullptr)
    {
        transformB = transformA;
        m_ComponentsB = m_ComponentsA;
    }
    else
    {
        transformB = m_Target->worldTransform();
        if (sourceSpace() == TransformSpace::local)
        {
            Mat2D inverse;
            if (!getParentWorld(*m_Target).invert(&inverse))
            {
                return;
            }
            transformB = inverse * transformB;
        }
        m_ComponentsB = transformB.decompose();

        if (!doesCopy())
        {
            m_ComponentsB.scaleX(destSpace() == TransformSpace::local ? 1.0f
                                                                      : m_ComponentsA.scaleX());
        }
        else
        {
            m_ComponentsB.scaleX(m_ComponentsB.scaleX() * copyFactor());
            if (offset())
            {
                m_ComponentsB.scaleX(m_ComponentsB.scaleX() * component->scaleX());
            }
        }

        if (!doesCopyY())
        {
            m_ComponentsB.scaleY(destSpace() == TransformSpace::local ? 1.0f
                                                                      : m_ComponentsA.scaleY());
        }
        else
        {
            m_ComponentsB.scaleY(m_ComponentsB.scaleY() * copyFactorY());
            if (offset())
            {
                m_ComponentsB.scaleY(m_ComponentsB.scaleY() * component->scaleY());
            }
        }

        if (destSpace() == TransformSpace::local)
        {
            // Destination space is in parent transform coordinates. Recompose
            // the parent local transform and get it in world, then decompose
            // the world for interpolation.

            transformB = Mat2D::compose(m_ComponentsB);
            transformB = getParentWorld(*component) * transformB;
            m_ComponentsB = transformB.decompose();
        }
    }

    bool clamplocal = minMaxSpace() == TransformSpace::local;
    if (clamplocal)
    {
        // Apply min max in local space, so transform to local coordinates
        // first.
        transformB = Mat2D::compose(m_ComponentsB);
        Mat2D inverse;
        if (!getParentWorld(*component).invert(&inverse))
        {
            return;
        }
        transformB = inverse * transformB;
        m_ComponentsB = transformB.decompose();
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
        transformB = Mat2D::compose(m_ComponentsB);
        transformB = getParentWorld(*component) * transformB;
        m_ComponentsB = transformB.decompose();
    }

    float t = strength();
    float ti = 1.0f - t;

    m_ComponentsB.rotation(m_ComponentsA.rotation());
    m_ComponentsB.x(m_ComponentsA.x());
    m_ComponentsB.y(m_ComponentsA.y());
    m_ComponentsB.scaleX(m_ComponentsA.scaleX() * ti + m_ComponentsB.scaleX() * t);
    m_ComponentsB.scaleY(m_ComponentsA.scaleY() * ti + m_ComponentsB.scaleY() * t);
    m_ComponentsB.skew(m_ComponentsA.skew());

    component->mutableWorldTransform() = Mat2D::compose(m_ComponentsB);
}

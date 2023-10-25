#include "rive/artboard.hpp"
#include "rive/command_path.hpp"
#include "rive/constraints/follow_path_constraint.hpp"
#include "rive/factory.hpp"
#include "rive/math/contour_measure.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/math/math_types.hpp"
#include "rive/shapes/metrics_path.hpp"
#include "rive/shapes/path.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/transform_component.hpp"
#include <algorithm>
#include <iostream>
#include <typeinfo>

using namespace rive;

static float positiveMod(float value, float range)
{
    assert(range > 0.0f);
    float v = fmodf(value, range);
    if (v < 0.0f)
    {
        v += range;
    }
    return v;
}

void FollowPathConstraint::distanceChanged() { markConstraintDirty(); }
void FollowPathConstraint::orientChanged() { markConstraintDirty(); }

const Mat2D FollowPathConstraint::targetTransform() const
{
    if (!m_Target->is<Shape>())
    {
        return m_Target->worldTransform();
    }

    float totalLength = 0.0f;
    for (auto contour : m_contours)
    {
        totalLength += contour->length();
    }

    float actualDistance = positiveMod(distance(), 1.0f);
    if (distance() != 0 && actualDistance == 0)
    {
        actualDistance = 1;
    }
    float distanceUnits = totalLength * std::min(1.0f, std::max(0.0f, actualDistance));
    float runningLength = 0;
    ContourMeasure::PosTan posTan;
    for (auto contour : m_contours)
    {
        float pathLength = contour->length();
        if (distanceUnits <= pathLength + runningLength)
        {
            posTan = contour->getPosTan(distanceUnits - runningLength);
            break;
        }
        runningLength += pathLength;
    }
    Vec2D position = Vec2D(posTan.pos.x, posTan.pos.y);
    Mat2D transformB = Mat2D(m_Target->worldTransform());

    if (orient())
    {
        transformB = Mat2D::fromRotation(std::atan2(posTan.tan.y, posTan.tan.x));
    }
    Vec2D offsetPosition = Vec2D();
    if (offset())
    {
        if (parent()->is<TransformComponent>())
        {
            Mat2D components = parent()->as<TransformComponent>()->transform();
            offsetPosition.x = components[4];
            offsetPosition.y = components[5];
        }
    }
    transformB[4] = position.x + offsetPosition.x;
    transformB[5] = position.y + offsetPosition.y;
    return transformB;
}

void FollowPathConstraint::constrain(TransformComponent* component)
{
    if (m_Target == nullptr)
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

    m_ComponentsA = transformA.decompose();
    m_ComponentsB = transformB.decompose();

    float t = strength();
    float ti = 1.0f - t;

    if (!orient())
    {
        float angleA = std::fmod(m_ComponentsA.rotation(), math::PI * 2);
        m_ComponentsB.rotation(angleA);
    }
    m_ComponentsB.x(m_ComponentsA.x() * ti + m_ComponentsB.x() * t);
    m_ComponentsB.y(m_ComponentsA.y() * ti + m_ComponentsB.y() * t);
    m_ComponentsB.scaleX(m_ComponentsA.scaleX());
    m_ComponentsB.scaleY(m_ComponentsA.scaleY());
    m_ComponentsB.skew(m_ComponentsA.skew());

    component->mutableWorldTransform() = Mat2D::compose(m_ComponentsB);
}

void FollowPathConstraint::update(ComponentDirt value)
{
    if (!m_Target->is<Shape>())
    {
        return;
    }

    Shape* shape = static_cast<Shape*>(m_Target);
    if (hasDirt(value, ComponentDirt::Path))
    {
        m_rawPath.rewind();
        m_contours.clear();
        for (auto path : shape->paths())
        {
            auto commandPath = static_cast<MetricsPath*>(path->commandPath());
            commandPath->addToRawPath(m_rawPath, path->pathTransform());
        }

        auto measure = ContourMeasureIter(m_rawPath);
        for (auto contour = measure.next(); contour != nullptr; contour = measure.next())
        {
            m_contours.push_back(contour);
        }
    }
}

StatusCode FollowPathConstraint::onAddedClean(CoreContext* context)
{
    if (m_Target != nullptr && m_Target->is<Shape>())
    {
        Shape* shape = static_cast<Shape*>(m_Target);
        shape->addDefaultPathSpace(PathSpace::FollowPath);
    }
    return Super::onAddedClean(context);
}

void FollowPathConstraint::buildDependencies()
{
    if (m_Target != nullptr && m_Target->is<Shape>()) // which should never happen
    {
        // Follow path should update after the target's path composer
        Shape* shape = static_cast<Shape*>(m_Target);
        shape->pathComposer()->addDependent(this);
    }
    // The constrained component should update after follow path
    addDependent(parent());
}

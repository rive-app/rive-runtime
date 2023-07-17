#include "rive/artboard.hpp"
#include "rive/command_path.hpp"
#include "rive/constraints/follow_path_constraint.hpp"
#include "rive/factory.hpp"
#include "rive/math/contour_measure.hpp"
#include "rive/shapes/metrics_path.hpp"
#include "rive/shapes/path.hpp"
#include "rive/shapes/shape.hpp"
#include <algorithm>
#include <iostream>
#include <typeinfo>

using namespace rive;

void FollowPathConstraint::distanceChanged() { markConstraintDirty(); }
void FollowPathConstraint::orientChanged() { markConstraintDirty(); }

const Mat2D FollowPathConstraint::targetTransform() const
{
    if (!m_Target->is<Shape>())
    {
        return m_Target->worldTransform();
    }
    MetricsPath* metricsPath = m_WorldPath.get();
    if (metricsPath == nullptr)
    {
        return m_Target->worldTransform();
    }

    const std::vector<MetricsPath*>& paths = metricsPath->paths();
    float totalLength = metricsPath->length();
    float distanceUnits = totalLength * std::min(1.0f, std::max(0.0f, distance()));
    float runningLength = 0;
    ContourMeasure::PosTan posTan;
    for (auto path : paths)
    {
        float pathLength = path->length();
        if (distanceUnits < pathLength + runningLength)
        {
            posTan = path->contourMeasure()->getPosTan(distanceUnits - runningLength);
            break;
        }
        runningLength += pathLength;
    }
    Vec2D position = Vec2D(posTan.pos.x, posTan.pos.y);

    Mat2D transformB = Mat2D(m_Target->worldTransform());
    transformB[4] = position.x;
    transformB[5] = position.y;

    if (offset())
    {
        if (parent()->is<TransformComponent>())
        {
            transformB *= parent()->as<TransformComponent>()->transform();
        }
    }
    if (orient())
    {
        transformB *= Mat2D::fromRotation(std::atan2(posTan.tan.y, posTan.tan.x));
    }
    else
    {
        if (parent()->is<TransformComponent>())
        {
            auto comp = parent()->as<TransformComponent>()->worldTransform().decompose();
            transformB *= Mat2D::fromRotation(comp.rotation());
        }
    }
    return transformB;
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
        if (m_WorldPath == nullptr)
        {
            m_WorldPath = std::unique_ptr<MetricsPath>(new OnlyMetricsPath());
        }
        else
        {
            m_WorldPath->rewind();
        }
        for (auto path : shape->paths())
        {
            const Mat2D& transform = path->pathTransform();
            m_WorldPath->addPath(path->commandPath(), transform);
        }
    }
}

StatusCode FollowPathConstraint::onAddedClean(CoreContext* context)
{
    Shape* shape = static_cast<Shape*>(m_Target);
    shape->addDefaultPathSpace(PathSpace::FollowPath);
    return Super::onAddedClean(context);
}

void FollowPathConstraint::buildDependencies()
{
    assert(m_Target != nullptr);
    Super::buildDependencies();
    if (m_Target != nullptr && m_Target->is<Shape>()) // which should never happen
    {
        Shape* shape = static_cast<Shape*>(m_Target);
        shape->pathComposer()->addDependent(this);
    }
}

#include "rive/artboard.hpp"
#include "rive/command_path.hpp"
#include "rive/constraints/follow_path_constraint.hpp"
#include "rive/factory.hpp"
#include "rive/math/contour_measure.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/math/math_types.hpp"
#include "rive/shapes/path.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/transform_component.hpp"
#include <algorithm>
#include <iostream>
#include <typeinfo>

using namespace rive;

void FollowPathConstraint::distanceChanged() { markConstraintDirty(); }
void FollowPathConstraint::orientChanged() { markConstraintDirty(); }

const Mat2D FollowPathConstraint::targetTransform() const
{
    if (m_Target->is<Shape>() || m_Target->is<Path>())
    {
        auto result = m_pathMeasure.atPercentage(distance());
        Vec2D position = result.pos;
        Mat2D transformB = Mat2D(m_Target->worldTransform());

        if (orient())
        {
            transformB = Mat2D::fromRotation(std::atan2(result.tan.y, result.tan.x));
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
    else
    {
        return m_Target->worldTransform();
    }
}

void FollowPathConstraint::constrain(TransformComponent* component)
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
    std::vector<Path*> paths;
    if (m_Target->is<Shape>())
    {
        auto shape = m_Target->as<Shape>();
        for (auto path : shape->paths())
        {
            paths.push_back(path);
        }
    }
    else if (m_Target->is<Path>())
    {
        paths.push_back(m_Target->as<Path>());
    }
    if (paths.size() > 0)
    {
        m_rawPath.rewind();
        for (auto path : paths)
        {
            m_rawPath.addPath(path->rawPath(), &path->pathTransform());
        }

        m_pathMeasure = PathMeasure(&m_rawPath);
    }
}

StatusCode FollowPathConstraint::onAddedClean(CoreContext* context)
{
    if (m_Target != nullptr)
    {
        if (m_Target->is<Shape>())
        {
            Shape* shape = static_cast<Shape*>(m_Target);
            shape->addFlags(PathFlags::followPath);
        }
        else if (m_Target->is<Path>())
        {
            Path* path = static_cast<Path*>(m_Target);
            path->addFlags(PathFlags::followPath);
        }
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
    // ok this appears to be enough to get the inital layout & animations to be working.
    else if (m_Target != nullptr && m_Target->is<Path>()) // which should never happen
    {
        // or do we need to be dependent on the shape still???
        Path* path = static_cast<Path*>(m_Target);
        path->addDependent(this);
    }
    // The constrained component should update after follow path
    addDependent(parent());
}

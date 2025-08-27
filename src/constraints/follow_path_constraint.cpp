#include "rive/artboard.hpp"
#include "rive/command_path.hpp"
#include "rive/constraints/constrainable_list.hpp"
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

const Mat2D FollowPathConstraint::targetTransform(float distanceOffset) const
{
    if (m_Target->is<Shape>() || m_Target->is<Path>())
    {
        auto result = m_pathMeasure.atPercentage(distance() * distanceOffset);
        Vec2D position = result.pos;
        Mat2D transformB = Mat2D(m_Target->worldTransform());

        if (orient())
        {
            transformB =
                Mat2D::fromRotation(std::atan2(result.tan.y, result.tan.x));
        }
        Vec2D offsetPosition = Vec2D();
        if (offset())
        {
            if (parent()->is<TransformComponent>())
            {
                Mat2D components =
                    parent()->as<TransformComponent>()->transform();
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

void FollowPathConstraint::constrainList(ConstrainableList* list)
{
    auto listTransform = list->listTransform();
    std::vector<Mat2D*> transforms;
    list->listItemTransforms(transforms);
    auto count = transforms.size();
    float offsetDistance = count <= 1 ? 0 : 1 / ((float)count - 1);
    for (int i = 0; i < count; i++)
    {
        auto transform = transforms[i];
        auto transformComponents =
            constrainAtOffset(*transform, listTransform, i * offsetDistance);
        auto transformB = Mat2D::compose(transformComponents);
        transform->xx(transformB.xx());
        transform->xy(transformB.xy());
        transform->yx(transformB.yx());
        transform->yy(transformB.yy());
        transform->tx(transformB.tx());
        transform->ty(transformB.ty());
    }
}

TransformComponents FollowPathConstraint::constrainAtOffset(
    const Mat2D& componentTransform,
    const Mat2D& parentTransform,
    float componentOffset)
{
    if (m_Target == nullptr || m_Target->isCollapsed())
    {
        return TransformComponents();
    }
    Mat2D transformB(targetTransform(componentOffset));
    auto transformComponents =
        constrainHelper(componentTransform, transformB, parentTransform);
    return transformComponents;
}

void FollowPathConstraint::constrain(TransformComponent* component)
{
    if (m_Target == nullptr || m_Target->isCollapsed())
    {
        return;
    }
    Mat2D transformB(targetTransform());
    const Mat2D& targetParentWorld = getParentWorld(*component);
    auto transformComponents = constrainHelper(component->worldTransform(),
                                               transformB,
                                               targetParentWorld);
    component->mutableWorldTransform() = Mat2D::compose(transformComponents);
}

TransformComponents FollowPathConstraint::constrainHelper(
    const Mat2D& componentTransform,
    Mat2D& transformB,
    const Mat2D& componentParentWorld)
{
    const Mat2D& transformA = componentTransform;
    if (sourceSpace() == TransformSpace::local)
    {
        const Mat2D& targetParentWorld = getParentWorld(*m_Target);

        Mat2D inverse;
        if (!targetParentWorld.invert(&inverse))
        {
            TransformComponents result;
            return result;
        }
        transformB = inverse * transformB;
    }
    if (destSpace() == TransformSpace::local)
    {
        transformB = componentParentWorld * transformB;
    }

    auto componentsA = transformA.decompose();
    auto componentsB = transformB.decompose();

    float t = strength();
    float ti = 1.0f - t;

    if (!orient())
    {
        float angleA = std::fmod(componentsA.rotation(), math::PI * 2);
        componentsB.rotation(angleA);
    }
    componentsB.x(componentsA.x() * ti + componentsB.x() * t);
    componentsB.y(componentsA.y() * ti + componentsB.y() * t);
    componentsB.scaleX(componentsA.scaleX());
    componentsB.scaleY(componentsA.scaleY());
    componentsB.skew(componentsA.skew());
    return componentsB;
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

    if (m_Target != nullptr &&
        m_Target->is<Shape>()) // which should never happen
    {
        // Follow path should update after the target's path composer
        Shape* shape = static_cast<Shape*>(m_Target);
        shape->pathComposer()->addDependent(this);
    }
    // ok this appears to be enough to get the inital layout & animations to be
    // working.
    else if (m_Target != nullptr &&
             m_Target->is<Path>()) // which should never happen
    {
        // or do we need to be dependent on the shape still???
        Path* path = static_cast<Path*>(m_Target);
        path->addDependent(this);
    }
    // The constrained component should update after follow path
    addDependent(parent());
    auto constrainableList = ConstrainableList::from(parent());
    if (constrainableList != nullptr)
    {
        constrainableList->addListConstraint(this);
    }
}

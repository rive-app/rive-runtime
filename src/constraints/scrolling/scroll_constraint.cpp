#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/constraints/scrolling/scroll_constraint_proxy.hpp"
#include "rive/constraints/transform_constraint.hpp"
#include "rive/core_context.hpp"
#include "rive/transform_component.hpp"
#include "rive/math/mat2d.hpp"

using namespace rive;

void ScrollConstraint::constrain(TransformComponent* component)
{
    m_scrollTransform =
        Mat2D::fromTranslate(constrainsHorizontal() ? clampedOffsetX() : 0,
                             constrainsVertical() ? clampedOffsetY() : 0);
}

void ScrollConstraint::constrainChild(LayoutComponent* component)
{
    auto targetTransform =
        Mat2D::multiply(component->worldTransform(), m_scrollTransform);
    TransformConstraint::constrainWorld(component,
                                        component->worldTransform(),
                                        m_componentsA,
                                        targetTransform,
                                        m_componentsB,
                                        strength());
}

void ScrollConstraint::dragView(Vec2D delta)
{
    if (m_physics != nullptr)
    {
        m_physics->accumulate(delta);
    }
    offsetX(offsetX() + delta.x);
    offsetY(offsetY() + delta.y);
}

void ScrollConstraint::runPhysics()
{
    std::vector<Vec2D> snappingPoints;
    if (snap())
    {
        for (auto child : content()->children())
        {
            if (child->is<LayoutComponent>())
            {
                auto c = child->as<LayoutComponent>();
                snappingPoints.push_back(Vec2D(c->layoutX(), c->layoutY()));
            }
        }
    }
    if (m_physics != nullptr)
    {
        m_physics->run(Vec2D(maxOffsetX(), maxOffsetY()),
                       Vec2D(offsetX(), offsetY()),
                       snap() ? snappingPoints : std::vector<Vec2D>());
    }
}

bool ScrollConstraint::advanceComponent(float elapsedSeconds,
                                        AdvanceFlags flags)
{
    if ((flags & AdvanceFlags::AdvanceNested) != AdvanceFlags::AdvanceNested)
    {
        offsetX(0);
        offsetY(0);
        return false;
    }
    if (m_physics == nullptr)
    {
        return false;
    }
    if (m_physics->isRunning())
    {
        auto offset = m_physics->advance(elapsedSeconds);
        offsetX(offset.x);
        offsetY(offset.y);
    }
    return m_physics->enabled();
}

std::vector<DraggableProxy*> ScrollConstraint::draggables()
{
    std::vector<DraggableProxy*> items;
    items.push_back(new ViewportDraggableProxy(this, viewport()));
    return items;
}

void ScrollConstraint::buildDependencies()
{
    Super::buildDependencies();
    for (auto child : content()->children())
    {
        if (child->is<LayoutComponent>())
        {
            addDependent(child);
            child->as<LayoutComponent>()->addLayoutConstraint(
                static_cast<LayoutConstraint*>(this));
        }
    }
}

Core* ScrollConstraint::clone() const
{
    auto cloned = ScrollConstraintBase::clone();
    if (physics() != nullptr)
    {
        auto constraint = cloned->as<ScrollConstraint>();
        auto clonedPhysics = physics()->clone()->as<ScrollPhysics>();
        constraint->physics(clonedPhysics);
    }
    return cloned;
}

StatusCode ScrollConstraint::import(ImportStack& importStack)
{
    auto backboardImporter =
        importStack.latest<BackboardImporter>(BackboardBase::typeKey);
    if (backboardImporter != nullptr)
    {
        std::vector<ScrollPhysics*> physicsObjects =
            backboardImporter->physics();
        if (physicsId() != -1 && physicsId() < physicsObjects.size())
        {
            auto phys = physicsObjects[physicsId()];
            if (phys != nullptr)
            {
                auto cloned = phys->clone()->as<ScrollPhysics>();
                physics(cloned);
            }
        }
    }
    else
    {
        return StatusCode::MissingObject;
    }
    return Super::import(importStack);
}

void ScrollConstraint::initPhysics()
{
    if (m_physics != nullptr)
    {
        m_physics->prepare(direction());
    }
}
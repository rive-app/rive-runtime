#include "rive/constraints/scroll_constraint.hpp"
#include "rive/constraints/scroll_constraint_proxy.hpp"
#include "rive/constraints/transform_constraint.hpp"
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
    if (m_physicsX != nullptr)
    {
        m_physicsX->accumulate(delta.x);
    }
    if (m_physicsY != nullptr)
    {
        m_physicsY->accumulate(delta.y);
    }
    offsetX(offsetX() + delta.x);
    offsetY(offsetY() + delta.y);
}

void ScrollConstraint::runPhysics()
{
    std::vector<float> snappingPoints;
    bool isHorizontal = content()->mainAxisIsRow();
    if (snap())
    {
        for (auto child : content()->children())
        {
            if (child->is<LayoutComponent>())
            {
                auto c = child->as<LayoutComponent>();
                snappingPoints.push_back(isHorizontal ? c->layoutX()
                                                      : c->layoutY());
            }
        }
    }
    if (m_physicsX != nullptr)
    {
        m_physicsX->run(maxOffsetX(),
                        offsetX(),
                        (snap() && isHorizontal) ? snappingPoints
                                                 : std::vector<float>());
    }
    if (m_physicsY != nullptr)
    {
        m_physicsY->run(maxOffsetY(),
                        offsetY(),
                        (snap() && !isHorizontal) ? snappingPoints
                                                  : std::vector<float>());
    }
}

bool ScrollConstraint::advanceComponent(float elapsedSeconds,
                                        AdvanceFlags flags)
{
    if ((flags & AdvanceFlags::Animate) != AdvanceFlags::Animate)
    {
        return false;
    }
    auto physicsX = m_physicsX;
    auto physicsY = m_physicsY;
    if (physicsX == nullptr && physicsY == nullptr)
    {
        return false;
    }
    if (physicsX != nullptr && physicsX->isRunning())
    {
        offsetX(physicsX->advance(elapsedSeconds));
        if (physicsX->isRunning() != true)
        {
            m_physicsX = nullptr;
        }
    }
    if (physicsY != nullptr && physicsY->isRunning())
    {
        offsetY(physicsY->advance(elapsedSeconds));
        if (physicsY->isRunning() != true)
        {
            m_physicsY = nullptr;
        }
    }
    return m_physicsX != nullptr || m_physicsY != nullptr;
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

void ScrollConstraint::initPhysics()
{
    if (constrainsHorizontal())
    {
        physicsX(new MelaScrollPhysics());
    }
    if (constrainsVertical())
    {
        physicsY(new MelaScrollPhysics());
    }
}
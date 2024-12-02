#ifndef _RIVE_SCROLL_CONSTRAINT_HPP_
#define _RIVE_SCROLL_CONSTRAINT_HPP_
#include "rive/generated/constraints/scroll_constraint_base.hpp"
#include "rive/advance_flags.hpp"
#include "rive/advancing_component.hpp"
#include "rive/constraints/layout_constraint.hpp"
#include "rive/constraints/scroll_physics.hpp"
#include "rive/layout_component.hpp"
#include "rive/math/math_types.hpp"
#include "rive/math/transform_components.hpp"
#include <stdio.h>
namespace rive
{
enum class ScrollPhysicsType : uint8_t
{
    mela,
    defaultPhysics
};
class ScrollConstraint : public ScrollConstraintBase,
                         public AdvancingComponent,
                         public LayoutConstraint
{
private:
    TransformComponents m_componentsA;
    TransformComponents m_componentsB;
    float m_offsetX = 0;
    float m_offsetY = 0;
    ScrollPhysics* m_physicsX;
    ScrollPhysics* m_physicsY;
    Mat2D m_scrollTransform;

public:
    void constrain(TransformComponent* component) override;
    std::vector<DraggableProxy*> draggables() override;
    void buildDependencies() override;
    void dragView(Vec2D delta);
    void runPhysics();
    void constrainChild(LayoutComponent* component) override;
    bool advanceComponent(float elapsedSeconds,
                          AdvanceFlags flags = AdvanceFlags::Animate |
                                               AdvanceFlags::NewFrame) override;

    ScrollPhysics* physicsX() { return m_physicsX; }
    ScrollPhysics* physicsY() { return m_physicsY; }
    void physicsX(ScrollPhysics* physics) { m_physicsX = physics; }
    void physicsY(ScrollPhysics* physics) { m_physicsY = physics; }
    void initPhysics();

    LayoutComponent* content() { return parent()->as<LayoutComponent>(); }
    LayoutComponent* viewport()
    {
        return parent()->parent()->as<LayoutComponent>();
    }
    float contentWidth() { return content()->layoutWidth(); }
    float contentHeight() { return content()->layoutHeight(); }
    float viewportWidth()
    {
        return direction() == DraggableConstraintDirection::vertical
                   ? viewport()->layoutWidth()
                   : std::max(0.0f,
                              viewport()->layoutWidth() - content()->layoutX());
    }
    float viewportHeight()
    {
        return direction() == DraggableConstraintDirection::horizontal
                   ? viewport()->layoutHeight()
                   : std::max(0.0f,
                              viewport()->layoutHeight() -
                                  content()->layoutY());
    }
    float visibleWidthRatio()
    {
        if (contentWidth() == 0)
        {
            return 1;
        }
        return std::min(1.0f, viewportWidth() / contentWidth());
    }
    float visibleHeightRatio()
    {
        if (contentHeight() == 0)
        {
            return 1;
        }
        return std::min(1.0f, viewportHeight() / contentHeight());
    }
    float maxOffsetX()
    {
        if (contentWidth() == 0)
        {
            return 0;
        }
        return viewportWidth() - contentWidth() - viewport()->paddingRight();
    }
    float maxOffsetY()
    {
        if (contentHeight() == 0)
        {
            return 0;
        }
        return viewportHeight() - contentHeight() - viewport()->paddingBottom();
    }
    float clampedOffsetX()
    {
        if (maxOffsetX() > 0)
        {
            return 0;
        }
        if (m_physicsX != nullptr)
        {
            return m_physicsX->clamp(maxOffsetX(), m_offsetX);
        }
        return math::clamp(m_offsetX, maxOffsetX(), 0);
    }
    float clampedOffsetY()
    {
        if (maxOffsetY() > 0)
        {
            return 0;
        }
        if (m_physicsY != nullptr)
        {
            return m_physicsY->clamp(maxOffsetY(), m_offsetY);
        }
        return math::clamp(m_offsetY, maxOffsetY(), 0);
    }

    float offsetX() { return m_offsetX; }
    float offsetY() { return m_offsetY; }
    void offsetX(float value)
    {
        if (m_offsetX == value)
        {
            return;
        }
        m_offsetX = value;
        content()->markWorldTransformDirty();
    }
    void offsetY(float value)
    {
        if (m_offsetY == value)
        {
            return;
        }
        m_offsetY = value;
        content()->markWorldTransformDirty();
    }
};
} // namespace rive

#endif
#ifndef _RIVE_SCROLL_CONSTRAINT_HPP_
#define _RIVE_SCROLL_CONSTRAINT_HPP_
#include "rive/generated/constraints/scrolling/scroll_constraint_base.hpp"
#include "rive/advance_flags.hpp"
#include "rive/advancing_component.hpp"
#include "rive/constraints/layout_constraint.hpp"
#include "rive/constraints/scrolling/scroll_physics.hpp"
#include "rive/layout_component.hpp"
#include "rive/math/math_types.hpp"
#include "rive/math/transform_components.hpp"
#include <stdio.h>
namespace rive
{
class LayoutNodeProvider;
class ScrollVirtualizer;

class ScrollConstraint : public ScrollConstraintBase,
                         public AdvancingComponent,
                         public LayoutConstraint
{
private:
    TransformComponents m_componentsA;
    TransformComponents m_componentsB;
    float m_offsetX = 0;
    float m_offsetY = 0;
    ScrollPhysics* m_physics;
    Mat2D m_scrollTransform;
    bool m_isDragging = false;
    ScrollVirtualizer* m_virtualizer = nullptr;
    std::vector<LayoutNodeProvider*> m_layoutChildren;
    int m_childConstraintAppliedCount = 0;

    Vec2D positionAtIndex(float index);
    float indexAtPosition(Vec2D pos);
    float maxOffsetXForPercent();
    float maxOffsetYForPercent();

public:
    ~ScrollConstraint();
    void constrain(TransformComponent* component) override;
    std::vector<DraggableProxy*> draggables() override;
    void buildDependencies() override;
    StatusCode import(ImportStack& importStack) override;
    StatusCode onAddedDirty(CoreContext* context) override;
    Core* clone() const override;
    void dragView(Vec2D delta, float timeStamp);
    void runPhysics();
    void constrainChild(LayoutNodeProvider* child) override;
    void addLayoutChild(LayoutNodeProvider* child) override;
    Constraint* constraint() override { return this; }
    void constrainVirtualized(bool force = false);
    bool advanceComponent(float elapsedSeconds,
                          AdvanceFlags flags = AdvanceFlags::Animate |
                                               AdvanceFlags::NewFrame) override;

    ScrollPhysics* physics() const { return m_physics; }
    void physics(ScrollPhysics* physics) { m_physics = physics; }
    void initPhysics();
    void stopPhysics();

    ScrollPhysicsType physicsType() const
    {
        return ScrollPhysicsType(physicsTypeValue());
    }

    LayoutComponent* content() { return parent()->as<LayoutComponent>(); }
    LayoutComponent* viewport()
    {
        return parent()->parent()->as<LayoutComponent>();
    }
    float contentWidth();
    float contentHeight();
    float viewportWidth();
    float viewportHeight();
    float visibleWidthRatio();
    float visibleHeightRatio();
    float minOffsetX();
    float minOffsetY();
    float maxOffsetX();
    float maxOffsetY();
    float clampedOffsetX();
    float clampedOffsetY();

    float offsetX() { return m_offsetX; }
    float offsetY() { return m_offsetY; }
    void offsetX(float value);
    void offsetY(float value);

    void scrollOffsetXChanged() override { offsetX(scrollOffsetX()); }
    void scrollOffsetYChanged() override { offsetY(scrollOffsetY()); }

    float scrollPercentX() override;
    float scrollPercentY() override;
    float scrollIndex() override;
    void setScrollPercentX(float value) override;
    void setScrollPercentY(float value) override;
    void setScrollIndex(float value) override;

    size_t scrollItemCount();
    std::vector<LayoutNodeProvider*>& scrollChildren()
    {
        return m_layoutChildren;
    }
    Vec2D gap();
    bool mainAxisIsColumn();
};
} // namespace rive

#endif
#ifndef _RIVE_SCROLL_BAR_CONSTRAINT_HPP_
#define _RIVE_SCROLL_BAR_CONSTRAINT_HPP_
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/math/transform_components.hpp"
#include "rive/generated/constraints/scrolling/scroll_bar_constraint_base.hpp"
#include <stdio.h>
namespace rive
{
class DraggableProxy;
class TransformComponent;
class TransformComponents;

class ScrollBarConstraint : public ScrollBarConstraintBase
{
private:
    TransformComponents m_componentsA;
    TransformComponents m_componentsB;
    ScrollConstraint* m_scrollConstraint;

public:
    void constrain(TransformComponent* component) override;
    std::vector<DraggableProxy*> draggables() override;
    ScrollConstraint* scrollConstraint() { return m_scrollConstraint; }
    void scrollConstraint(ScrollConstraint* constraint)
    {
        m_scrollConstraint = constraint;
    }
    void buildDependencies() override;
    StatusCode onAddedDirty(CoreContext* context) override;
    void dragThumb(Vec2D delta);
    void hitTrack(Vec2D worldPosition);
    LayoutComponent* thumb() { return parent()->as<LayoutComponent>(); }
    LayoutComponent* track()
    {
        return parent()->parent()->as<LayoutComponent>();
    }
    float computedThumbWidth();
    float computedThumbHeight();
    bool validate(CoreContext* context) override;
};
} // namespace rive

#endif
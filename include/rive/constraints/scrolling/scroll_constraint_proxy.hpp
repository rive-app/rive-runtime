#ifndef _RIVE_SCROLL_CONSTRAINT_PROXY_HPP_
#define _RIVE_SCROLL_CONSTRAINT_PROXY_HPP_
#include <stdio.h>
#include "rive/constraints/draggable_constraint.hpp"
namespace rive
{
class ScrollConstraint;
class Drawable;

class ViewportDraggableProxy : public DraggableProxy
{
private:
    ScrollConstraint* m_constraint;
    Vec2D m_lastPosition;

public:
    ViewportDraggableProxy(ScrollConstraint* constraint, Drawable* hittable) :
        m_constraint(constraint)
    {
        m_hittable = hittable;
    }
    ~ViewportDraggableProxy() {}
    bool isOpaque() override { return false; }
    void startDrag(Vec2D mousePosition) override;
    void drag(Vec2D mousePosition) override;
    void endDrag(Vec2D mousePosition) override;
};
} // namespace rive

#endif
#ifndef _RIVE_SCROLL_BAR_CONSTRAINT_PROXY_HPP_
#define _RIVE_SCROLL_BAR_CONSTRAINT_PROXY_HPP_
#include <stdio.h>
#include "rive/constraints/draggable_constraint.hpp"
namespace rive
{
class ScrollBarConstraint;
class Drawable;

class ThumbDraggableProxy : public DraggableProxy
{
private:
    ScrollBarConstraint* m_constraint;
    Vec2D m_lastPosition;

public:
    ThumbDraggableProxy(ScrollBarConstraint* constraint, Drawable* hittable) :
        m_constraint(constraint)
    {
        m_hittable = hittable;
    }
    ~ThumbDraggableProxy() {}
    bool isOpaque() override { return true; }
    void startDrag(Vec2D mousePosition) override;
    void drag(Vec2D mousePosition) override;
    void endDrag(Vec2D mousePosition) override {}
};

class TrackDraggableProxy : public DraggableProxy
{
private:
    ScrollBarConstraint* m_constraint;

public:
    TrackDraggableProxy(ScrollBarConstraint* constraint, Drawable* hittable) :
        m_constraint(constraint)
    {
        m_hittable = hittable;
    }
    ~TrackDraggableProxy() {}
    bool isOpaque() override { return true; }
    void startDrag(Vec2D mousePosition) override;
    void drag(Vec2D mousePosition) override {}
    void endDrag(Vec2D mousePosition) override {}
};
} // namespace rive

#endif
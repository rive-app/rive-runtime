#ifndef _RIVE_DRAGGABLE_CONSTRAINT_HPP_
#define _RIVE_DRAGGABLE_CONSTRAINT_HPP_
#include "rive/generated/constraints/draggable_constraint_base.hpp"
#include "rive/drawable.hpp"
#include "rive/math/vec2d.hpp"
#include <stdio.h>
namespace rive
{
enum class DraggableConstraintDirection : uint8_t
{
    horizontal,
    vertical,
    all
};

class DraggableProxy
{
protected:
    Drawable* m_hittable;

public:
    virtual ~DraggableProxy() {}
    virtual bool isOpaque() { return false; }
    virtual bool startDrag(Vec2D mousePosition, float timeStamp = 0) = 0;
    virtual bool drag(Vec2D mousePosition, float timeStamp = 0) = 0;
    virtual bool endDrag(Vec2D mousePosition, float timeStamp = 0) = 0;
    Drawable* hittable() { return m_hittable; }
};

class DraggableConstraint : public DraggableConstraintBase
{
public:
    DraggableConstraint() {}
    virtual std::vector<DraggableProxy*> draggables() = 0;

    DraggableConstraintDirection direction()
    {
        return DraggableConstraintDirection(directionValue());
    }
    bool constrainsHorizontal()
    {
        auto dir = direction();
        return dir == DraggableConstraintDirection::horizontal ||
               dir == DraggableConstraintDirection::all;
    }
    bool constrainsVertical()
    {
        auto dir = direction();
        return dir == DraggableConstraintDirection::vertical ||
               dir == DraggableConstraintDirection::all;
    }
};
} // namespace rive

#endif
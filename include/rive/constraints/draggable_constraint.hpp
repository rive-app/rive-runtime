#ifndef _RIVE_DRAGGABLE_CONSTRAINT_HPP_
#define _RIVE_DRAGGABLE_CONSTRAINT_HPP_
#include "rive/generated/constraints/draggable_constraint_base.hpp"
#include "rive/animation/state_machine_listener.hpp"
#include "rive/drawable.hpp"
#include "rive/listener_group.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/process_event_result.hpp"
#include <stdio.h>
namespace rive
{
class HitComponent;

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

class DraggableConstraint : public DraggableConstraintBase,
                            public ListenerGroupProvider
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
    std::vector<ListenerGroupWithTargets*> listenerGroups() override;
    std::vector<HitComponent*> hitComponents(StateMachineInstance* sm) override
    {
        return {};
    }
};

class DraggableConstraintListenerGroup : public ListenerGroup
{
public:
    DraggableConstraintListenerGroup(const StateMachineListener* listener,
                                     DraggableConstraint* constraint,
                                     DraggableProxy* draggable) :
        ListenerGroup(listener),
        m_constraint(constraint),
        m_draggable(draggable)
    {}
    ~DraggableConstraintListenerGroup()
    {
        delete listener();
        delete m_draggable;
    }

    void enable(int pointerId = 0) override {}
    void disable(int pointerId = 0) override {}

    DraggableConstraint* constraint() { return m_constraint; }

    bool canEarlyOut(Component* drawable) override { return false; }

    bool needsDownListener(Component* drawable) override { return true; }

    bool needsUpListener(Component* drawable) override { return true; }
    ProcessEventResult processEvent(
        Component* component,
        Vec2D position,
        int pointerId,
        ListenerType hitEvent,
        bool canHit,
        float timeStamp,
        StateMachineInstance* stateMachineInstance) override;

    bool cancelPointer(int pointerId, Vec2D position, float timeStamp) override;

private:
    DraggableConstraint* m_constraint;
    DraggableProxy* m_draggable;
    // The pointer whose drag has actually scrolled, or -1. Held by id rather
    // than as a bool: cancellation walks every tracked pointer, and the dragEnd
    // belongs to the pointer that scrolled, not to whichever is visited first.
    int m_scrollingPointerId = -1;
};
} // namespace rive

#endif
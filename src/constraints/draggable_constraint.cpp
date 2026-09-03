#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_listener_single.hpp"
#include "rive/component.hpp"
#include "rive/constraints/draggable_constraint.hpp"

using namespace rive;

std::vector<ListenerGroupWithTargets*> DraggableConstraint::listenerGroups()
{
    std::vector<ListenerGroupWithTargets*> result;
    for (auto dragProxy : draggables())
    {
        auto listener = new StateMachineListenerSingle();
        listener->listenerTypeValue(
            static_cast<uint32_t>(ListenerType::componentProvided));
        auto* listenerGroup =
            new DraggableConstraintListenerGroup(listener, this, dragProxy);
        auto hittable = dragProxy->hittable();
        if (hittable != nullptr && hittable->is<Component>())
        {
            auto* target =
                new HitTarget(hittable->as<Component>(), dragProxy->isOpaque());
            auto* groupWithTargets =
                new ListenerGroupWithTargets(listenerGroup,
                                             std::vector<HitTarget*>{target});
            result.push_back(groupWithTargets);
        }
    }
    return result;
}

// Mirrors the down -> clicked/out teardown in processEvent, which cancellation
// bypasses by clearing the phase before processEvent can observe the change.
bool DraggableConstraintListenerGroup::cancelPointer(int pointerId,
                                                     Vec2D position,
                                                     float timeStamp)
{
    auto pointer = findPointerData(pointerId);
    bool wasDown =
        pointer != nullptr && pointer->phase == GestureClickPhase::down;
    bool wasDragging =
        ListenerGroup::cancelPointer(pointerId, position, timeStamp);
    if (wasDown)
    {
        m_draggable->endDrag(position, timeStamp);
        if (m_scrollingPointerId == pointerId)
        {
            m_scrollingPointerId = -1;
            wasDragging = true;
        }
    }
    return wasDragging;
}

ProcessEventResult DraggableConstraintListenerGroup::processEvent(
    Component* component,
    Vec2D position,
    int pointerId,
    ListenerType hitEvent,
    bool canHit,
    float timeStamp,
    StateMachineInstance* stateMachineInstance)
{
    auto pointer = pointerData(pointerId);
    auto prevPhase = pointer->phase;
    ListenerGroup::processEvent(component,
                                position,
                                pointerId,
                                hitEvent,
                                canHit,
                                timeStamp,
                                stateMachineInstance);
    if (prevPhase == GestureClickPhase::down &&
        (pointer->phase == GestureClickPhase::clicked ||
         pointer->phase == GestureClickPhase::out))
    {
        m_draggable->endDrag(position, timeStamp);
        if (m_scrollingPointerId == pointerId)
        {
            stateMachineInstance->dragEnd(position, timeStamp, pointerId);
            m_scrollingPointerId = -1;
            return ProcessEventResult::scroll;
        }
    }
    else if (prevPhase != GestureClickPhase::down &&
             pointer->phase == GestureClickPhase::down)
    {
        m_draggable->startDrag(position, timeStamp);
        m_scrollingPointerId = -1;
    }
    else if (hitEvent == ListenerType::move &&
             pointer->phase == GestureClickPhase::down)
    {
        auto hasDragged = m_draggable->drag(position, timeStamp);
        if (hasDragged)
        {
            if (m_scrollingPointerId != pointerId)
            {
                stateMachineInstance->dragStart(position,
                                                timeStamp,
                                                false,
                                                pointerId);
            }
            m_scrollingPointerId = pointerId;
            return ProcessEventResult::scroll;
        }
    }
    return ProcessEventResult::none;
}
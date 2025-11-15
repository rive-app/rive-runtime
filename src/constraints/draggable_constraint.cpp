#include "rive/animation/state_machine_instance.hpp"
#include "rive/component.hpp"
#include "rive/constraints/draggable_constraint.hpp"

using namespace rive;

std::vector<ListenerGroupWithTargets*> DraggableConstraint::listenerGroups()
{
    std::vector<ListenerGroupWithTargets*> result;
    for (auto dragProxy : draggables())
    {
        auto listener = new StateMachineListener();
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
        if (m_hasScrolled)
        {
            stateMachineInstance->dragEnd(position, timeStamp, pointerId);
            m_hasScrolled = false;
            return ProcessEventResult::scroll;
        }
    }
    else if (prevPhase != GestureClickPhase::down &&
             pointer->phase == GestureClickPhase::down)
    {
        m_draggable->startDrag(position, timeStamp);
        m_hasScrolled = false;
    }
    else if (hitEvent == ListenerType::move &&
             pointer->phase == GestureClickPhase::down)
    {
        auto hasDragged = m_draggable->drag(position, timeStamp);
        if (hasDragged)
        {
            if (!m_hasScrolled)
            {
                stateMachineInstance->dragStart(position,
                                                timeStamp,
                                                false,
                                                pointerId);
            }
            m_hasScrolled = true;
            return ProcessEventResult::scroll;
        }
    }
    return ProcessEventResult::none;
}
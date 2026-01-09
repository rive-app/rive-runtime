#include "rive/animation/state_machine_listener.hpp"
#include "rive/constraints/scrolling/scroll_bar_constraint.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/listener_group.hpp"
#include "rive/scripted/scripted_drawable.hpp"
#include "rive/scripted/scripted_layout.hpp"

using namespace rive;

ListenerGroup::~ListenerGroup()
{
    for (auto& pointerData : m_pointers)
    {
        delete pointerData.second;
    }
    for (auto& pointerData : m_pointersPool)
    {
        delete pointerData;
    }
}

_PointerData* ListenerGroup::pointerData(int id)
{
    if (m_pointers.find(id) == m_pointers.end())
    {
        if (m_pointersPool.size() > 0)
        {
            m_pointers[id] = m_pointersPool.back();
            m_pointersPool.pop_back();
        }
        else
        {
            m_pointers[id] = new _PointerData();
        }
    }
    return m_pointers[id];
}

void ListenerGroup::hover(int id)
{
    auto pointer = pointerData(id);
    pointer->isHovered = true;
}

void ListenerGroup::reset(int pointerId)
{
    auto pointer = pointerData(pointerId);
    if (pointer->phase != GestureClickPhase::disabled)
    {
        m_isConsumed = false;
        pointer->isPrevHovered = pointer->isHovered;
        pointer->isHovered = false;
    }
    if (pointer->phase == GestureClickPhase::clicked)
    {
        pointer->phase = GestureClickPhase::out;
    }
}

void ListenerGroup::releaseEvent(int pointerId)
{
    if (m_pointers.find(pointerId) != m_pointers.end())
    {
        // Reset pointer data before caching it
        auto pointerData = m_pointers[pointerId];
        pointerData->isHovered = false;
        pointerData->isPrevHovered = false;
        pointerData->phase = GestureClickPhase::out;
        auto previousPosition = pointerData->previousPosition();
        previousPosition->x = 0;
        previousPosition->y = 0;

        m_pointersPool.push_back(m_pointers[pointerId]);
        m_pointers.erase(pointerId);
    }
}

void ListenerGroup::enable(int pointerId)
{
    auto pointer = pointerData(pointerId);
    pointer->phase = GestureClickPhase::out;
}

void ListenerGroup::disable(int pointerId)
{
    auto pointer = pointerData(pointerId);
    pointer->phase = GestureClickPhase::disabled;
    consume();
}

bool ListenerGroup::canEarlyOut(Component* drawable)
{
    auto listenerType = m_listener->listenerType();
    return !(listenerType == ListenerType::enter ||
             listenerType == ListenerType::exit ||
             listenerType == ListenerType::move ||
             listenerType == ListenerType::drag);
}

bool ListenerGroup::needsDownListener(Component* drawable)
{
    auto listenerType = m_listener->listenerType();
    return listenerType == ListenerType::down ||
           listenerType == ListenerType::click ||
           listenerType == ListenerType::drag;
}

bool ListenerGroup::needsUpListener(Component* drawable)
{
    auto listenerType = m_listener->listenerType();
    return listenerType == ListenerType::up ||
           listenerType == ListenerType::click ||
           listenerType == ListenerType::drag;
}

ProcessEventResult ListenerGroup::processEvent(
    Component* component,
    Vec2D position,
    int pointerId,
    ListenerType hitEvent,
    bool canHit,
    float timeStamp,
    StateMachineInstance* stateMachineInstance)
{
    // Because each group is tested individually for its hover state, a
    // group could be marked "incorrectly" as hovered at this point. But
    // once we iterate each element in the drawing order, that group can be
    // occluded by an opaque target on top  of it. So although it is hovered
    // in isolation, it shouldn't be considered as hovered in the full
    // context. In this case, we unhover the group so it is not marked as
    // previously hovered.
    auto pointer = pointerData(pointerId);
    auto prevPhase = pointer->phase;
    if (!canHit && pointer->isHovered)
    {
        pointer->isHovered = false;
    }

    bool isGroupHovered = canHit ? pointer->isHovered : false;
    bool hoverChange = pointer->isPrevHovered != isGroupHovered;
    // If hover has changes, it means that the element is hovered for the
    // first time. Previous positions need to be reset to avoid jumps.
    auto previousPosition = pointer->previousPosition();
    if (hoverChange && isGroupHovered)
    {
        previousPosition->x = position.x;
        previousPosition->y = position.y;
    }

    // Handle click gesture phases. A click gesture has two phases.
    // First one attached to a pointer down actions, second one attached to
    // a pointer up action. Both need to act on a shape of the listener
    // group.
    if (isGroupHovered)
    {
        if (hitEvent == ListenerType::down)
        {
            pointer->phase = GestureClickPhase::down;
        }
        else if (hitEvent == ListenerType::up &&
                 pointer->phase == GestureClickPhase::down)
        {
            pointer->phase = GestureClickPhase::clicked;
        }
    }
    else
    {
        if (hitEvent == ListenerType::down || hitEvent == ListenerType::up)
        {
            pointer->phase = GestureClickPhase::out;
        }
    }
    if (prevPhase == GestureClickPhase::down &&
        (pointer->phase == GestureClickPhase::clicked ||
         pointer->phase == GestureClickPhase::out) &&
        m_hasDragged)
    {
        stateMachineInstance->dragEnd(position, timeStamp, pointerId);
        m_hasDragged = false;
    }
    auto _listener = listener();
    // Always update hover states regardless of which specific listener type
    // we're trying to trigger.
    // If hover has changed and:
    // - it's hovering and the listener is of type enter
    // - it's not hovering and the listener is of type exit
    if (hoverChange &&
        ((isGroupHovered && _listener->listenerType() == ListenerType::enter) ||
         (!isGroupHovered && _listener->listenerType() == ListenerType::exit)))
    {
        _listener->performChanges(
            stateMachineInstance,
            position,
            Vec2D(previousPosition->x, previousPosition->y));
        stateMachineInstance->markNeedsAdvance();
        consume();
    }
    // Perform changes if:
    // - the click gesture is complete and the listener is of type click
    // - the event type matches the listener type and it is hovering the
    // group
    if ((pointer->phase == GestureClickPhase::clicked &&
         _listener->listenerType() == ListenerType::click) ||
        (isGroupHovered && hitEvent == _listener->listenerType()))
    {
        _listener->performChanges(
            stateMachineInstance,
            position,
            Vec2D(previousPosition->x, previousPosition->y));
        stateMachineInstance->markNeedsAdvance();
        consume();
    }
    // Perform changes if:
    // - the listener type is drag
    // - the clickPhase is down
    // - the pointer type is move
    if (pointer->phase == GestureClickPhase::down &&
        _listener->listenerType() == ListenerType::drag &&
        hitEvent == ListenerType::move)
    {
        _listener->performChanges(
            stateMachineInstance,
            position,
            Vec2D(previousPosition->x, previousPosition->y));
        stateMachineInstance->markNeedsAdvance();
        if (!m_hasDragged)
        {
            stateMachineInstance->dragStart(position,
                                            timeStamp,
                                            false,
                                            pointerId);
            m_hasDragged = true;
        }
        consume();
    }
    previousPosition->x = position.x;
    previousPosition->y = position.y;
    return ProcessEventResult::pointer;
}

ListenerGroupProvider* ListenerGroupProvider::from(Core* component)
{
    switch (component->coreType())
    {
        case ScrollConstraint::typeKey:
            return component->as<ScrollConstraint>();
        case ScrollBarConstraint::typeKey:
            return component->as<ScrollBarConstraint>();
        case ScriptedLayout::typeKey:
            return component->as<ScriptedLayout>();
        case ScriptedDrawable::typeKey:
            return component->as<ScriptedDrawable>();
    }
    return nullptr;
}

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
    auto* listenerGroup = new DraggableConstraintListenerGroup(
        listener,
        this,
        dragProxy);
    auto hittable = dragProxy->hittable();
    if (hittable != nullptr && hittable->is<Component>())
    {
      auto* target = new HitTarget(
          hittable->as<Component>(),
          dragProxy->isOpaque());
      auto* groupWithTargets = new ListenerGroupWithTargets(
          listenerGroup,
          std::vector<HitTarget*>{target});
      result.push_back(groupWithTargets);
    }
  }
  return result;
}

// Cancellation clears the phase before processEvent sees the release, so the
// drag ends here. No hand-off: cancelPointers cancels every pointer at once.
bool DraggableConstraintListenerGroup::cancelPointer(
    int pointerId,
    Vec2D position,
    float timeStamp)
{
  auto pointer = findPointerData(pointerId);
  bool wasDown = pointer != nullptr &&
                 pointer->phase == GestureClickPhase::down;
  bool wasDragging = ListenerGroup::cancelPointer(
      pointerId,
      position,
      timeStamp);
  if (wasDown && isDragPointer(pointerId))
  {
    m_draggable->endDrag(position, timeStamp);
    m_hasDragPointer = false;
  }
  return wasDragging;
}

ProcessEventResult DraggableConstraintListenerGroup::processEvent(
    Component* component,
    Vec2D position,
    int pointerId,
    ListenerType hitEvent,
    PointerButton button,
    bool canHit,
    float timeStamp,
    StateMachineInstance* stateMachineInstance)
{
  if ((hitEvent == ListenerType::down || hitEvent == ListenerType::up) &&
      button != PointerButton::primary)
  {
    return ProcessEventResult::none;
  }
  auto pointer = pointerData(pointerId);
  auto prevPhase = pointer->phase;
  bool hadDragged = pointer->hasDragged;
  // Dispatches the dragEnd for a pointer that had dragged and is now released.
  ListenerGroup::processEvent(
      component,
      position,
      pointerId,
      hitEvent,
      button,
      canHit,
      timeStamp,
      stateMachineInstance);
  // With another finger down, the drag's finger leaving ends the drag with no
  // velocity, and the first other finger to move takes it. A lone exit ends
  // nothing: it also comes for a pointer passing under something drawn above.
  if (hitEvent == ListenerType::exit && isDragPointer(pointerId) &&
      pointer->phase == GestureClickPhase::down)
  {
    if (hasPressedPointerOtherThan(pointerId))
    {
      m_draggable->restartDrag(position, timeStamp);
      m_draggable->endDrag(position, timeStamp);
      m_hasDragPointer = false;
    }
  }
  else if (
      prevPhase == GestureClickPhase::down &&
      (pointer->phase == GestureClickPhase::clicked ||
       pointer->phase == GestureClickPhase::out))
  {
    if (isDragPointer(pointerId))
    {
      if (hasPressedPointerOtherThan(pointerId))
      {
        m_draggable->restartDrag(position, timeStamp);
      }
      m_draggable->endDrag(position, timeStamp);
      m_hasDragPointer = false;
    }
    if (hadDragged)
    {
      return ProcessEventResult::scroll;
    }
  }
  else if (
      prevPhase != GestureClickPhase::down &&
      pointer->phase == GestureClickPhase::down)
  {
    // The newest finger takes the drag, from where it lands.
    auto owner = m_hasDragPointer && m_dragPointerId != pointerId
                     ? findPointerData(m_dragPointerId)
                     : nullptr;
    if (owner != nullptr && owner->phase == GestureClickPhase::down)
    {
      m_draggable->restartDrag(position, timeStamp);
    }
    else
    {
      m_draggable->startDrag(position, timeStamp);
    }
    m_hasDragPointer = true;
    m_dragPointerId = pointerId;
  }
  else if (
      hitEvent == ListenerType::move &&
      pointer->phase == GestureClickPhase::down)
  {
    auto owner = m_hasDragPointer ? findPointerData(m_dragPointerId) : nullptr;
    if (owner == nullptr || owner->phase != GestureClickPhase::down)
    {
      m_hasDragPointer = true;
      m_dragPointerId = pointerId;
      m_draggable->restartDrag(position, timeStamp);
      owner = pointer;
    }
    // Another finger that moves while the view scrolls stops pressing.
    bool hasDragged = false;
    if (m_dragPointerId == pointerId)
    {
      hasDragged = m_draggable->drag(position, timeStamp);
    }
    else
    {
      hasDragged = owner->hasDragged;
    }
    if (hasDragged)
    {
      if (!pointer->hasDragged)
      {
        stateMachineInstance->dragStart(position, timeStamp, false, pointerId);
        pointer->hasDragged = true;
      }
      return ProcessEventResult::scroll;
    }
  }
  return ProcessEventResult::none;
}

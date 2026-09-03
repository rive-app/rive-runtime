#ifndef _RIVE_LISTENER_GROUP_HPP_
#define _RIVE_LISTENER_GROUP_HPP_

#include <vector>
#include <unordered_map>
#include "rive/animation/state_machine_instance.hpp"
#include "rive/core.hpp"
#include "rive/gesture_click_phase.hpp"
#include "rive/listener_type.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/process_event_result.hpp"

namespace rive
{
class Component;
class HitComponent;
class StateMachineListener;

class _PointerData
{
public:
    bool isHovered = false;
    bool isPrevHovered = false;
    // Whether this pointer has actually dragged since it went down. Per pointer
    // rather than per group: the phase it is tested against is per pointer too,
    // so a group-wide flag lets one pointer end another's drag.
    bool hasDragged = false;
    GestureClickPhase phase = GestureClickPhase::out;
    Vec2D* previousPosition() { return &m_previousPosition; }

private:
    // A vector storing the previous position for this pointer data
    Vec2D m_previousPosition = Vec2D(0, 0);
};

class ListenerGroup
{
public:
    ListenerGroup(const StateMachineListener* listener) : m_listener(listener)
    {}
    virtual ~ListenerGroup();
    _PointerData* pointerData(int id);
    void consume() { m_isConsumed = true; }
    void hover(int id);
    void reset(int pointerId);
    void releaseEvent(int pointerId);
    /// Ends whatever gesture pointerId has in flight. Returns true when a drag
    /// was live for it, leaving the caller to dispatch the matching dragEnd
    /// once every group has been cancelled.
    virtual bool cancelPointer(int pointerId, Vec2D position, float timeStamp);
    /// Cancels every pointer this group is tracking, appending to dragEnded the
    /// ids that still owe a dragEnd.
    void cancelPointers(Vec2D position,
                        float timeStamp,
                        std::vector<int>& dragEnded);
    virtual void enable(int pointerId = 0);
    virtual void disable(int pointerId = 0);
    bool isConsumed() { return m_isConsumed; }
    virtual bool canEarlyOut(Component* drawable);
    virtual bool needsDownListener(Component* drawable);
    virtual bool needsUpListener(Component* drawable);

    virtual ProcessEventResult processEvent(
        Component* component,
        Vec2D position,
        int pointerId,
        ListenerType hitEvent,
        bool canHit,
        float timeStamp,
        StateMachineInstance* stateMachineInstance);
    const StateMachineListener* listener() const { return m_listener; };

protected:
    /// The data for an already-tracked pointer, or nullptr. Unlike
    /// pointerData() this never starts tracking one.
    _PointerData* findPointerData(int id);

private:
    // Consumed listeners aren't processed again in the current frame
    bool m_isConsumed = false;
    const StateMachineListener* m_listener;
    std::unordered_map<int, _PointerData*> m_pointers;
    std::vector<_PointerData*> m_pointersPool;
};

class HitTarget
{
private:
    Component* m_component = nullptr;
    bool m_isOpaque = false;

public:
    HitTarget(Component* component, bool isOpaque) :
        m_component(component), m_isOpaque(isOpaque)
    {}
    Component* component() { return m_component; }
    bool isOpaque() { return m_isOpaque; }
};

class ListenerGroupWithTargets
{
private:
    ListenerGroup* m_group = nullptr;
    std::vector<HitTarget*> m_targets;

public:
    ListenerGroupWithTargets(ListenerGroup* group,
                             std::vector<HitTarget*> targets) :
        m_group(group), m_targets(targets)
    {}
    ListenerGroup* group() { return m_group; }
    std::vector<HitTarget*> targets() { return m_targets; }
};

class ListenerGroupProvider
{
public:
    static ListenerGroupProvider* from(Core* component);
    virtual std::vector<ListenerGroupWithTargets*> listenerGroups() = 0;
    virtual std::vector<HitComponent*> hitComponents(
        StateMachineInstance* sm) = 0;
};

} // namespace rive
#endif

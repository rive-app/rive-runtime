#ifndef _RIVE_LISTENER_ACTION_HPP_
#define _RIVE_LISTENER_ACTION_HPP_
#include "rive/animation/listener_invocation.hpp"
#include "rive/animation/state_machine_fire_action.hpp"
#include "rive/generated/animation/listener_action_base.hpp"

namespace rive
{
class StateMachineInstance;
class ListenerAction : public ListenerActionBase
{
public:
    enum class ParentKind : uint32_t
    {
        Listener = 0,
        Transition = 1,
        State = 2,
    };

    StatusCode import(ImportStack& importStack) override;
    virtual void perform(StateMachineInstance* stateMachineInstance,
                         const ListenerInvocation& invocation) const = 0;

    /// Layer-scheduled actions: bit 0 matches [StateMachineFireOccurance]
    /// (0 = atStart, 1 = atEnd). Other bits are reserved.
    bool matchesScheduledOccurrence(StateMachineFireOccurance occurs) const
    {
        return (flags() & 1u) == static_cast<uint32_t>(occurs);
    }

    ParentKind parentKind() const
    {
        auto raw = (flags() >> 1) & 0x3u;
        return raw <= static_cast<uint32_t>(ParentKind::State)
                   ? static_cast<ParentKind>(raw)
                   : ParentKind::Listener;
    }
};
} // namespace rive

#endif
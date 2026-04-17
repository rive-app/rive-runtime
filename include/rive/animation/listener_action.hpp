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
    StatusCode import(ImportStack& importStack) override;
    virtual void perform(StateMachineInstance* stateMachineInstance,
                         const ListenerInvocation& invocation) const = 0;

    /// Layer-scheduled actions: bit 0 matches [StateMachineFireOccurance]
    /// (0 = atStart, 1 = atEnd). Other bits are reserved.
    bool matchesScheduledOccurrence(StateMachineFireOccurance occurs) const
    {
        return (flags() & 1u) == static_cast<uint32_t>(occurs);
    }
};
} // namespace rive

#endif
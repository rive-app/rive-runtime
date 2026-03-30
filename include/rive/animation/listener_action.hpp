#ifndef _RIVE_LISTENER_ACTION_HPP_
#define _RIVE_LISTENER_ACTION_HPP_
#include "rive/animation/listener_invocation.hpp"
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
};
} // namespace rive

#endif
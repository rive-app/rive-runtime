#ifndef _RIVE_FOCUS_ACTION_TARGET_HPP_
#define _RIVE_FOCUS_ACTION_TARGET_HPP_
#include "rive/generated/animation/focus_action_target_base.hpp"
namespace rive
{
class FocusActionTarget : public FocusActionTargetBase
{
public:
    void perform(StateMachineInstance* stateMachineInstance,
                 const ListenerInvocation& invocation) const override;
};
} // namespace rive

#endif
#ifndef _RIVE_FOCUS_ACTION_CLEAR_HPP_
#define _RIVE_FOCUS_ACTION_CLEAR_HPP_
#include "rive/generated/animation/focus_action_clear_base.hpp"
namespace rive
{
class FocusActionClear : public FocusActionClearBase
{
public:
    void perform(StateMachineInstance* stateMachineInstance,
                 const ListenerInvocation& invocation) const override;
};
} // namespace rive

#endif

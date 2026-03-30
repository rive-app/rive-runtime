/*
 * Copyright 2024 Rive
 */

#ifndef _RIVE_FOCUS_ACTION_HPP_
#define _RIVE_FOCUS_ACTION_HPP_
#include "rive/generated/animation/focus_action_base.hpp"

namespace rive
{
class FocusAction : public FocusActionBase
{
public:
    void perform(StateMachineInstance* stateMachineInstance,
                 const ListenerInvocation& invocation) const override;
};
} // namespace rive

#endif
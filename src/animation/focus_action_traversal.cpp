/*
 * Copyright 2024 Rive
 */

#include "rive/animation/focus_action_traversal.hpp"
#include "rive/animation/listener_invocation.hpp"
#include "rive/animation/state_machine_instance.hpp"

using namespace rive;

void FocusActionTraversal::perform(StateMachineInstance* stateMachineInstance,
                                   const ListenerInvocation& invocation) const
{
    (void)invocation;
    if (stateMachineInstance == nullptr)
    {
        return;
    }

    stateMachineInstance->queueFocusTraversal(traversalKind());
}

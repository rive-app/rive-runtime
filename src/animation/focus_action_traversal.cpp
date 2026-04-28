/*
 * Copyright 2024 Rive
 */

#include "rive/animation/focus_action_traversal.hpp"
#include "rive/animation/listener_invocation.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/input/focus_manager.hpp"

using namespace rive;

void FocusActionTraversal::perform(StateMachineInstance* stateMachineInstance,
                                   const ListenerInvocation& invocation) const
{
    (void)invocation;
    if (stateMachineInstance == nullptr)
    {
        return;
    }

    FocusManager* manager = stateMachineInstance->focusManager();
    // traversalKind: 0=next, 1=previous, 2=up, 3=down, 4=left, 5=right
    switch (traversalKind())
    {
        case 0:
            manager->focusNext();
            break;
        case 1:
            manager->focusPrevious();
            break;
        case 2:
            manager->focusUp();
            break;
        case 3:
            manager->focusDown();
            break;
        case 4:
            manager->focusLeft();
            break;
        case 5:
            manager->focusRight();
            break;
        default:
            manager->focusNext();
            break;
    }
}

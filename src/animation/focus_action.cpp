/*
 * Copyright 2024 Rive
 */

#include "rive/animation/focus_action.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/focus_data.hpp"
#include "rive/node.hpp"

using namespace rive;

void FocusAction::perform(StateMachineInstance* stateMachineInstance,
                          Vec2D position,
                          Vec2D previousPosition,
                          int pointerId) const
{
    auto target = stateMachineInstance->artboard()->resolve(targetId());
    if (target == nullptr || !target->is<Node>())
    {
        return;
    }

    auto node = target->as<Node>();
    FocusData* focusData = nullptr;

    // Find FocusData child of the node
    for (auto child : node->children())
    {
        if (child->is<FocusData>())
        {
            focusData = child->as<FocusData>();
            break;
        }
    }

    if (focusData != nullptr)
    {
        stateMachineInstance->setFocus(focusData);
    }
}

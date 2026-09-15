/*
 * Copyright 2024 Rive
 */

#include "rive/animation/transition_focus_condition.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/transition_condition_op.hpp"
#include "rive/animation/transition_property_component_comparator.hpp"
#include "rive/artboard.hpp"
#include "rive/focus_data.hpp"
#include "rive/input/focus_manager.hpp"
#include "rive/node.hpp"

using namespace rive;

bool TransitionFocusCondition::evaluate(
    const StateMachineInstance* stateMachineInstance,
    StateMachineLayerInstance* layerInstance) const
{
    (void)layerInstance;
    if (stateMachineInstance == nullptr)
    {
        return false;
    }

    // The target focusable component is stored on a component comparator. In
    // the editor it lives on the right (the left side is the implicit "current
    // focus"), but on runtime import the single exported comparator is assigned
    // to the left slot, so accept it from either side.
    TransitionComparator* comparator = rightComparator();
    if (comparator == nullptr ||
        !comparator->is<TransitionPropertyComponentComparator>())
    {
        comparator = leftComparator();
    }
    if (comparator == nullptr ||
        !comparator->is<TransitionPropertyComponentComparator>())
    {
        return false;
    }
    auto focusComparator =
        comparator->as<TransitionPropertyComponentComparator>();

    const FocusManager* manager = stateMachineInstance->focusManager();
    if (manager == nullptr)
    {
        return false;
    }

    auto target =
        stateMachineInstance->artboard()->resolve(focusComparator->objectId());

    bool focused = false;
    if (target != nullptr && target->is<Node>())
    {
        auto node = target->as<Node>();
        for (auto child : node->children())
        {
            if (child->is<FocusData>())
            {
                auto focusNode = child->as<FocusData>()->focusNode();
                // Use hasFocus (not hasPrimaryFocus): the target is considered
                // focused when it or any descendant is the primary focus, so a
                // scope/container counts as focused while a child leaf holds
                // focus.
                focused = focusNode != nullptr && manager->hasFocus(focusNode);
                break;
            }
        }
    }

    return op() == TransitionConditionOp::equal ? focused : !focused;
}

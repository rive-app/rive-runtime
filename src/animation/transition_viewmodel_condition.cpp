#include "rive/animation/transition_viewmodel_condition.hpp"
#include "rive/animation/state_transition.hpp"
#include "rive/importers/state_transition_importer.hpp"
#include "rive/importers/state_machine_importer.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/component_dirt.hpp"

using namespace rive;

bool TransitionViewModelCondition::evaluate(const StateMachineInstance* stateMachineInstance) const
{
    if (leftComparator() != nullptr && rightComparator() != nullptr)
    {
        return leftComparator()->compare(rightComparator(), op(), stateMachineInstance);
    }
    return false;
}
#include "rive/animation/transition_input_condition.hpp"
#include "rive/importers/state_machine_importer.hpp"
#include "rive/animation/state_machine.hpp"

using namespace rive;

StatusCode TransitionInputCondition::import(ImportStack& importStack)
{
    auto stateMachineImporter = importStack.latest<StateMachineImporter>(StateMachine::typeKey);
    if (stateMachineImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }

    // Make sure the inputId doesn't overflow the input buffer.
    if ((size_t)inputId() >= stateMachineImporter->stateMachine()->inputCount())
    {
        return StatusCode::InvalidObject;
    }
    if (!validateInputType(stateMachineImporter->stateMachine()->input((size_t)inputId())))
    {
        return StatusCode::InvalidObject;
    }

    return Super::import(importStack);
}
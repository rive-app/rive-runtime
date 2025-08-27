#include "rive/animation/blend_state_1d_input.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_number.hpp"
#include "rive/animation/blend_state_1d_instance.hpp"
#include "rive/importers/state_machine_importer.hpp"

using namespace rive;

StatusCode BlendState1DInput::import(ImportStack& importStack)
{
    auto stateMachineImporter =
        importStack.latest<StateMachineImporter>(StateMachine::typeKey);
    if (stateMachineImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }

    if (hasValidInputId())
    {
        // Make sure the inputId doesn't overflow the input buffer.
        if ((size_t)inputId() >=
            stateMachineImporter->stateMachine()->inputCount())
        {
            return StatusCode::InvalidObject;
        }
        auto input =
            stateMachineImporter->stateMachine()->input((size_t)inputId());
        if (input == nullptr || !input->is<StateMachineNumber>())
        {
            return StatusCode::InvalidObject;
        }
    }
    return Super::import(importStack);
}

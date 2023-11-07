#include "rive/animation/listener_number_change.hpp"
#include "rive/animation/nested_input.hpp"
#include "rive/animation/nested_number.hpp"
#include "rive/animation/nested_state_machine.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_number.hpp"
#include "rive/animation/state_machine_input_instance.hpp"

using namespace rive;

bool ListenerNumberChange::validateInputType(const StateMachineInput* input) const
{
    // A null input is valid as the StateMachine can attempt to limp along if we
    // introduce new input types that old conditions are expected to handle in
    // newer runtimes. The older runtimes will just evaluate them to true.
    return input == nullptr || input->is<StateMachineNumber>();
}

bool ListenerNumberChange::validateNestedInputType(const NestedInput* input) const
{
    // A null nested input is valid as the StateMachine can attempt to limp along if we
    // introduce new input types that old conditions are expected to handle in
    // newer runtimes. The older runtimes will just evaluate them to true.
    return input == nullptr || input->is<NestedNumber>();
}

void ListenerNumberChange::perform(StateMachineInstance* stateMachineInstance, Vec2D position) const
{
    if (nestedInputId() != Core::emptyId)
    {
        auto nestedInputInstance = stateMachineInstance->artboard()->resolve(nestedInputId());
        if (nestedInputInstance == nullptr)
        {
            return;
        }
        auto nestedNumberInput = static_cast<NestedNumber*>(nestedInputInstance);
        if (nestedNumberInput != nullptr)
        {
            nestedNumberInput->nestedValue(value());
        }
    }
    else
    {
        auto inputInstance = stateMachineInstance->input(inputId());
        if (inputInstance == nullptr)
        {
            return;
        }
        // If it's not null, it must be our correct type (why we validate at load time).
        auto numberInput = static_cast<SMINumber*>(inputInstance);
        if (numberInput != nullptr)
        {
            numberInput->value(value());
        }
    }
}

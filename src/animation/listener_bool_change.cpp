#include "rive/animation/listener_bool_change.hpp"
#include "rive/animation/nested_bool.hpp"
#include "rive/animation/nested_input.hpp"
#include "rive/animation/nested_state_machine.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_bool.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
using namespace rive;

bool ListenerBoolChange::validateInputType(const StateMachineInput* input) const
{
    // A null input is valid as the StateMachine can attempt to limp along if we
    // introduce new input types that old conditions are expected to handle in
    // newer runtimes. The older runtimes will just evaluate them to true.
    return input == nullptr || input->is<StateMachineBool>();
}

bool ListenerBoolChange::validateNestedInputType(const NestedInput* input) const
{
    // A null nested input is valid as the StateMachine can attempt to limp along if we
    // introduce new input types that old conditions are expected to handle in
    // newer runtimes. The older runtimes will just evaluate them to true.
    return input == nullptr || input->is<NestedBool>();
}

void ListenerBoolChange::perform(StateMachineInstance* stateMachineInstance, Vec2D position) const
{
    if (nestedInputId() != Core::emptyId)
    {
        auto nestedInputInstance = stateMachineInstance->artboard()->resolve(nestedInputId());
        if (nestedInputInstance == nullptr)
        {
            return;
        }
        auto nestedBoolInput = static_cast<NestedBool*>(nestedInputInstance);
        if (nestedBoolInput != nullptr)
        {
            switch (value())
            {
                case 0:
                    nestedBoolInput->nestedValue(false);
                    break;
                case 1:
                    nestedBoolInput->nestedValue(true);
                    break;
                default:
                    nestedBoolInput->nestedValue(!nestedBoolInput->nestedValue());
                    break;
            }
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
        auto boolInput = static_cast<SMIBool*>(inputInstance);
        if (boolInput != nullptr)
        {
            switch (value())
            {
                case 0:
                    boolInput->value(false);
                    break;
                case 1:
                    boolInput->value(true);
                    break;
                default:
                    boolInput->value(!boolInput->value());
                    break;
            }
        }
    }
}

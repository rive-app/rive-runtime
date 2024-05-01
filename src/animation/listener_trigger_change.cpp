#include "rive/animation/listener_trigger_change.hpp"
#include "rive/animation/nested_input.hpp"
#include "rive/animation/nested_trigger.hpp"
#include "rive/animation/nested_state_machine.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_trigger.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/core/field_types/core_callback_type.hpp"

using namespace rive;

bool ListenerTriggerChange::validateInputType(const StateMachineInput* input) const
{
    // A null input is valid as the StateMachine can attempt to limp along if we
    // introduce new input types that old conditions are expected to handle in
    // newer runtimes. The older runtimes will just evaluate them to true.
    return input == nullptr || input->is<StateMachineTrigger>();
}

bool ListenerTriggerChange::validateNestedInputType(const NestedInput* input) const
{
    // A null nested input is valid as the StateMachine can attempt to limp along if we
    // introduce new input types that old conditions are expected to handle in
    // newer runtimes. The older runtimes will just evaluate them to true.
    return input == nullptr || input->is<NestedTrigger>();
}

void ListenerTriggerChange::perform(StateMachineInstance* stateMachineInstance,
                                    Vec2D position,
                                    Vec2D previousPosition) const
{
    if (nestedInputId() != Core::emptyId)
    {
        auto nestedInputInstance = stateMachineInstance->artboard()->resolve(nestedInputId());
        if (nestedInputInstance == nullptr)
        {
            return;
        }
        auto nestedTriggerInput = static_cast<NestedTrigger*>(nestedInputInstance);
        if (nestedTriggerInput != nullptr)
        {
            nestedTriggerInput->fire(CallbackData(stateMachineInstance, 0));
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
        auto triggerInput = static_cast<SMITrigger*>(inputInstance);
        if (triggerInput != nullptr)
        {
            triggerInput->fire();
        }
    }
}

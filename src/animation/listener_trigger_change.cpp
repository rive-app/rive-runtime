#include "rive/animation/listener_trigger_change.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_trigger.hpp"
#include "rive/animation/state_machine_input_instance.hpp"

using namespace rive;

bool ListenerTriggerChange::validateInputType(const StateMachineInput* input) const {
    // A null input is valid as the StateMachine can attempt to limp along if we
    // introduce new input types that old conditions are expected to handle in
    // newer runtimes. The older runtimes will just evaluate them to true.
    return input == nullptr || input->is<StateMachineTrigger>();
}

void ListenerTriggerChange::perform(StateMachineInstance* stateMachineInstance,
                                    Vec2D position) const {
    auto inputInstance = stateMachineInstance->input(inputId());
    if (inputInstance == nullptr) {
        return;
    }
    // If it's not null, it must be our correct type (why we validate at load time).
    auto triggerInput = reinterpret_cast<SMITrigger*>(inputInstance);
    triggerInput->fire();
}
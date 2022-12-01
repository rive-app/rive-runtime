#include "rive/animation/transition_trigger_condition.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine_trigger.hpp"
#include "rive/animation/transition_condition_op.hpp"

using namespace rive;

bool TransitionTriggerCondition::validateInputType(const StateMachineInput* input) const
{
    // A null input is valid as the StateMachine can attempt to limp along if we
    // introduce new input types that old conditions are expected to handle in
    // newer runtimes. The older runtimes will just evaluate them to true.
    return input == nullptr || input->is<StateMachineTrigger>();
}

bool TransitionTriggerCondition::evaluate(const SMIInput* inputInstance) const
{
    if (inputInstance == nullptr)
    {
        return true;
    }
    auto triggerInput = static_cast<const SMITrigger*>(inputInstance);

    if (triggerInput->m_Fired)
    {
        return true;
    }
    return false;
}

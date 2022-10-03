#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine_bool.hpp"
#include "rive/animation/state_machine_number.hpp"
#include "rive/animation/state_machine_trigger.hpp"
#include "rive/animation/state_machine_instance.hpp"

using namespace rive;

SMIInput::SMIInput(const StateMachineInput* input, StateMachineInstance* machineInstance) :
    m_MachineInstance(machineInstance), m_Input(input)
{}

uint16_t SMIInput::inputCoreType() const { return m_Input->coreType(); }

const std::string& SMIInput::name() const { return m_Input->name(); }

void SMIInput::valueChanged() { m_MachineInstance->markNeedsAdvance(); }

// bool

SMIBool::SMIBool(const StateMachineBool* input, StateMachineInstance* machineInstance) :
    SMIInput(input, machineInstance), m_Value(input->value())
{}

void SMIBool::value(bool newValue)
{
    if (m_Value == newValue)
    {
        return;
    }
    m_Value = newValue;
    valueChanged();
}

// number
SMINumber::SMINumber(const StateMachineNumber* input, StateMachineInstance* machineInstance) :
    SMIInput(input, machineInstance), m_Value(input->value())
{}

void SMINumber::value(float newValue)
{
    if (m_Value == newValue)
    {
        return;
    }
    m_Value = newValue;
    valueChanged();
}

// trigger
SMITrigger::SMITrigger(const StateMachineTrigger* input, StateMachineInstance* machineInstance) :
    SMIInput(input, machineInstance)
{}

void SMITrigger::fire()
{
    if (m_Fired)
    {
        return;
    }
    m_Fired = true;
    valueChanged();
}

#ifndef _RIVE_STATE_MACHINE_INPUT_INSTANCE_HPP_
#define _RIVE_STATE_MACHINE_INPUT_INSTANCE_HPP_

#include <string>
#include <stdint.h>

namespace rive
{
class StateMachineInstance;
class StateMachineInput;
class StateMachineBool;
class StateMachineNumber;
class StateMachineTrigger;
class TransitionTriggerCondition;
class StateMachineLayerInstance;

class SMIInput
{
    friend class StateMachineInstance;
    friend class StateMachineLayerInstance;

private:
    StateMachineInstance* m_MachineInstance;
    const StateMachineInput* m_Input;

    virtual void advanced() {}

protected:
    void valueChanged();

    SMIInput(const StateMachineInput* input, StateMachineInstance* machineInstance);

public:
    virtual ~SMIInput() {}
    const StateMachineInput* input() const { return m_Input; }

    const std::string& name() const;
    uint16_t inputCoreType() const;
};

class SMIBool : public SMIInput
{
    friend class StateMachineInstance;

private:
    bool m_Value;

    SMIBool(const StateMachineBool* input, StateMachineInstance* machineInstance);

public:
    bool value() const { return m_Value; }
    void value(bool newValue);
};

class SMINumber : public SMIInput
{
    friend class StateMachineInstance;

private:
    float m_Value;

    SMINumber(const StateMachineNumber* input, StateMachineInstance* machineInstance);

public:
    float value() const { return m_Value; }
    void value(float newValue);
};

class SMITrigger : public SMIInput
{
    friend class StateMachineInstance;
    friend class TransitionTriggerCondition;

private:
    bool m_Fired = false;

    SMITrigger(const StateMachineTrigger* input, StateMachineInstance* machineInstance);
    void advanced() override { m_Fired = false; }

public:
    void fire();

#ifdef TESTING
    bool didFire() { return m_Fired; }
#endif
};
} // namespace rive
#endif
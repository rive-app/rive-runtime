#ifndef _RIVE_STATE_MACHINE_INPUT_INSTANCE_HPP_
#define _RIVE_STATE_MACHINE_INPUT_INSTANCE_HPP_

#include <string>
#include <stdint.h>
#include <vector>
#include <algorithm>

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
    virtual void advanced() {}

protected:
    void valueChanged();

    SMIInput(const StateMachineInput* input,
             StateMachineInstance* machineInstance);

public:
    virtual ~SMIInput() {}
    const StateMachineInput* input() const { return m_input; }

    const std::string& name() const;
    uint16_t inputCoreType() const;

private:
    StateMachineInstance* m_machineInstance;
    const StateMachineInput* m_input;
#ifdef WITH_RIVE_TOOLS
    uint64_t m_index = 0;
#endif
};

class SMIBool : public SMIInput
{
    friend class StateMachineInstance;

private:
    bool m_Value;

    SMIBool(const StateMachineBool* input,
            StateMachineInstance* machineInstance);

public:
    bool value() const { return m_Value; }
    void value(bool newValue);
};

class SMINumber : public SMIInput
{
    friend class StateMachineInstance;

private:
    float m_Value;

    SMINumber(const StateMachineNumber* input,
              StateMachineInstance* machineInstance);

public:
    float value() const { return m_Value; }
    void value(float newValue);
};

class Triggerable
{

public:
    bool isUsedInLayer(StateMachineLayerInstance* layer) const
    {
        auto it = std::find(m_usedLayers.begin(), m_usedLayers.end(), layer);
        if (it == m_usedLayers.end())
        {
            return false;
        }
        return true;
    }
    void useInLayer(StateMachineLayerInstance* layer) const
    {
        auto it = std::find(m_usedLayers.begin(), m_usedLayers.end(), layer);
        if (it == m_usedLayers.end())
        {
            m_usedLayers.push_back(layer);
        }
    }

protected:
    mutable std::vector<StateMachineLayerInstance*> m_usedLayers;
};

class SMITrigger : public SMIInput, public Triggerable
{
    friend class StateMachineInstance;
    friend class TransitionTriggerCondition;
    bool m_fired = false;

    SMITrigger(const StateMachineTrigger* input,
               StateMachineInstance* machineInstance);
    void advanced() override
    {
        m_fired = false;
        m_usedLayers.clear();
    }

public:
    void fire();

#ifdef TESTING
    bool didFire() { return m_fired; }
#endif
};
} // namespace rive
#endif
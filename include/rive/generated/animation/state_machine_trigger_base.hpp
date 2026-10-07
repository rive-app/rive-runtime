#ifndef _RIVE_STATE_MACHINE_TRIGGER_BASE_HPP_
#define _RIVE_STATE_MACHINE_TRIGGER_BASE_HPP_
#include "rive/animation/state_machine_input.hpp"
namespace rive
{
class StateMachineTriggerBase : public StateMachineInput
{
protected:
    typedef StateMachineInput Super;

public:
    static const uint16_t typeKey = 58;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif
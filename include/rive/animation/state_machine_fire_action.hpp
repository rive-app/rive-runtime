#ifndef _RIVE_STATE_MACHINE_FIRE_ACTION_HPP_
#define _RIVE_STATE_MACHINE_FIRE_ACTION_HPP_
#include "rive/generated/animation/state_machine_fire_action_base.hpp"
#include <stdio.h>
namespace rive
{
class StateMachineInstance;
enum class StateMachineFireOccurance : int
{
    atStart = 0,
    atEnd = 1
};
class StateMachineFireAction : public StateMachineFireActionBase
{
public:
    StateMachineFireOccurance occurs() const
    {
        return (StateMachineFireOccurance)occursValue();
    }
    StatusCode import(ImportStack& importStack) override;
    virtual void perform(StateMachineInstance* stateMachineInstance) const = 0;
};
} // namespace rive

#endif
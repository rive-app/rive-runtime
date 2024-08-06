#ifndef _RIVE_TRANSITION_CONDITION_HPP_
#define _RIVE_TRANSITION_CONDITION_HPP_
#include "rive/generated/animation/transition_condition_base.hpp"
#include "rive/animation/state_machine_instance.hpp"

namespace rive
{
class StateMachineInput;
class SMIInput;

class TransitionCondition : public TransitionConditionBase
{
public:
    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode onAddedClean(CoreContext* context) override;

    StatusCode import(ImportStack& importStack) override;

    virtual bool evaluate(const StateMachineInstance* stateMachineInstance) const { return true; }

protected:
    virtual bool validateInputType(const StateMachineInput* input) const { return true; }
};
} // namespace rive

#endif
#ifndef _RIVE_TRANSITION_CONDITION_HPP_
#define _RIVE_TRANSITION_CONDITION_HPP_
#include "rive/generated/animation/transition_condition_base.hpp"

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

    virtual bool evaluate(const SMIInput* inputInstance) const { return true; }

protected:
    virtual bool validateInputType(const StateMachineInput* input) const { return true; }
};
} // namespace rive

#endif
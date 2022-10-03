#ifndef _RIVE_TRANSITION_VALUE_CONDITION_HPP_
#define _RIVE_TRANSITION_VALUE_CONDITION_HPP_
#include "rive/generated/animation/transition_value_condition_base.hpp"
#include "rive/animation/transition_condition_op.hpp"

namespace rive
{
class TransitionValueCondition : public TransitionValueConditionBase
{
public:
    TransitionConditionOp op() const { return (TransitionConditionOp)opValue(); }
};
} // namespace rive

#endif
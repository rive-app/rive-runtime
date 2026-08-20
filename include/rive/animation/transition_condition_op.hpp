#ifndef _RIVE_TRANSITION_CONDITION_OP_HPP_
#define _RIVE_TRANSITION_CONDITION_OP_HPP_

namespace rive
{
enum class TransitionConditionOp : uint8_t
{
    equal = 0,
    notEqual = 1,
    lessThanOrEqual = 2,
    greaterThanOrEqual = 3,
    lessThan = 4,
    greaterThan = 5
};
} // namespace rive

#endif
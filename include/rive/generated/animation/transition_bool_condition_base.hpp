#ifndef _RIVE_TRANSITION_BOOL_CONDITION_BASE_HPP_
#define _RIVE_TRANSITION_BOOL_CONDITION_BASE_HPP_
#include "rive/animation/transition_value_condition.hpp"
namespace rive
{
class TransitionBoolConditionBase : public TransitionValueCondition
{
protected:
    typedef TransitionValueCondition Super;

public:
    static const uint16_t typeKey = 71;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif
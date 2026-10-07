#ifndef _RIVE_TRANSITION_FOCUS_CONDITION_BASE_HPP_
#define _RIVE_TRANSITION_FOCUS_CONDITION_BASE_HPP_
#include "rive/animation/transition_viewmodel_condition.hpp"
namespace rive
{
class TransitionFocusConditionBase : public TransitionViewModelCondition
{
protected:
    typedef TransitionViewModelCondition Super;

public:
    static const uint16_t typeKey = 1038;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif
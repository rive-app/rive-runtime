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

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TransitionFocusConditionBase::typeKey:
            case TransitionViewModelConditionBase::typeKey:
            case TransitionConditionBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;

protected:
};
} // namespace rive

#endif

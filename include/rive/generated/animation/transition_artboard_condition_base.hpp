#ifndef _RIVE_TRANSITION_ARTBOARD_CONDITION_BASE_HPP_
#define _RIVE_TRANSITION_ARTBOARD_CONDITION_BASE_HPP_
#include "rive/animation/transition_viewmodel_condition.hpp"
namespace rive
{
class TransitionArtboardConditionBase : public TransitionViewModelCondition
{
protected:
    typedef TransitionViewModelCondition Super;

public:
    static const uint16_t typeKey = 497;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TransitionArtboardConditionBase::typeKey:
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
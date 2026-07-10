#ifndef _RIVE_FOCUS_ACTION_CLEAR_BASE_HPP_
#define _RIVE_FOCUS_ACTION_CLEAR_BASE_HPP_
#include "rive/animation/focus_action.hpp"
namespace rive
{
class FocusActionClearBase : public FocusAction
{
protected:
    typedef FocusAction Super;

public:
    static const uint16_t typeKey = 1037;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case FocusActionClearBase::typeKey:
            case FocusActionBase::typeKey:
            case ListenerActionBase::typeKey:
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
#ifndef _RIVE_FOCUS_ACTION_BASE_HPP_
#define _RIVE_FOCUS_ACTION_BASE_HPP_
#include "rive/animation/listener_action.hpp"
namespace rive
{
class FocusActionBase : public ListenerAction
{
protected:
    typedef ListenerAction Super;

public:
    static const uint16_t typeKey = 671;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case FocusActionBase::typeKey:
            case ListenerActionBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

protected:
};
} // namespace rive

#endif
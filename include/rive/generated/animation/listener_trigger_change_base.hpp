#ifndef _RIVE_LISTENER_TRIGGER_CHANGE_BASE_HPP_
#define _RIVE_LISTENER_TRIGGER_CHANGE_BASE_HPP_
#include "rive/animation/listener_input_change.hpp"
namespace rive
{
class ListenerTriggerChangeBase : public ListenerInputChange
{
protected:
    typedef ListenerInputChange Super;

public:
    static const uint16_t typeKey = 115;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ListenerTriggerChangeBase::typeKey:
            case ListenerInputChangeBase::typeKey:
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
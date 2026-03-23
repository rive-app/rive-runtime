#ifndef _RIVE_LISTENER_INPUT_TYPE_KEYBOARD_BASE_HPP_
#define _RIVE_LISTENER_INPUT_TYPE_KEYBOARD_BASE_HPP_
#include "rive/animation/listener_types/listener_input_type.hpp"
namespace rive
{
class ListenerInputTypeKeyboardBase : public ListenerInputType
{
protected:
    typedef ListenerInputType Super;

public:
    static const uint16_t typeKey = 665;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ListenerInputTypeKeyboardBase::typeKey:
            case ListenerInputTypeBase::typeKey:
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
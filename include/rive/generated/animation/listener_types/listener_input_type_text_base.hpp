#ifndef _RIVE_LISTENER_INPUT_TYPE_TEXT_BASE_HPP_
#define _RIVE_LISTENER_INPUT_TYPE_TEXT_BASE_HPP_
#include "rive/animation/listener_types/listener_input_type.hpp"
namespace rive
{
class ListenerInputTypeTextBase : public ListenerInputType
{
protected:
    typedef ListenerInputType Super;

public:
    static const uint16_t typeKey = 666;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ListenerInputTypeTextBase::typeKey:
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
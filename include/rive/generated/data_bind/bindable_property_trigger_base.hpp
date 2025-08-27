#ifndef _RIVE_BINDABLE_PROPERTY_TRIGGER_BASE_HPP_
#define _RIVE_BINDABLE_PROPERTY_TRIGGER_BASE_HPP_
#include "rive/data_bind/bindable_property_integer.hpp"
namespace rive
{
class BindablePropertyTriggerBase : public BindablePropertyInteger
{
protected:
    typedef BindablePropertyInteger Super;

public:
    static const uint16_t typeKey = 503;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case BindablePropertyTriggerBase::typeKey:
            case BindablePropertyIntegerBase::typeKey:
            case BindablePropertyBase::typeKey:
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
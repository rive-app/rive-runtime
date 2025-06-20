#ifndef _RIVE_BINDABLE_PROPERTY_ARTBOARD_BASE_HPP_
#define _RIVE_BINDABLE_PROPERTY_ARTBOARD_BASE_HPP_
#include "rive/data_bind/bindable_property_id.hpp"
namespace rive
{
class BindablePropertyArtboardBase : public BindablePropertyId
{
protected:
    typedef BindablePropertyId Super;

public:
    static const uint16_t typeKey = 597;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case BindablePropertyArtboardBase::typeKey:
            case BindablePropertyIdBase::typeKey:
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
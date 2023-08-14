#ifndef _RIVE_CUSTOM_PROPERTY_BASE_HPP_
#define _RIVE_CUSTOM_PROPERTY_BASE_HPP_
#include "rive/component.hpp"
namespace rive
{
class CustomPropertyBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 167;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case CustomPropertyBase::typeKey:
            case ComponentBase::typeKey:
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
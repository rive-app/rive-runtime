#ifndef _RIVE_BINDABLE_PROPERTY_BASE_HPP_
#define _RIVE_BINDABLE_PROPERTY_BASE_HPP_
#include "rive/core.hpp"
namespace rive
{
class BindablePropertyBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 9;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case BindablePropertyBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    void copy(const BindablePropertyBase& object) {}

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override { return false; }

protected:
};
} // namespace rive

#endif
#ifndef _RIVE_BINDABLE_PROPERTY_BASE_HPP_
#define _RIVE_BINDABLE_PROPERTY_BASE_HPP_
#include "rive/core.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class BindablePropertyBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 9;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
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

public:
    void copy(const BindablePropertyBase& object)
    {
        RIVE_EDITOR_COPY(object);
        RIVE_EDITOR_COPY_VALIDATED(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return false;
    }
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/data_bind/bindable_property_ext.inl"
#endif
};
} // namespace rive

#endif
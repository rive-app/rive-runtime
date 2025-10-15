#ifndef _RIVE_SCRIPT_INPUT_VIEW_MODEL_PROPERTY_BASE_HPP_
#define _RIVE_SCRIPT_INPUT_VIEW_MODEL_PROPERTY_BASE_HPP_
#include "rive/core/field_types/core_bytes_type.hpp"
#include "rive/custom_property.hpp"
#include "rive/span.hpp"
namespace rive
{
class ScriptInputViewModelPropertyBase : public CustomProperty
{
protected:
    typedef CustomProperty Super;

public:
    static const uint16_t typeKey = 612;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ScriptInputViewModelPropertyBase::typeKey:
            case CustomPropertyBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t dataBindPathIdsPropertyKey = 866;

public:
    virtual void decodeDataBindPathIds(Span<const uint8_t> value) = 0;
    virtual void copyDataBindPathIds(
        const ScriptInputViewModelPropertyBase& object) = 0;

    Core* clone() const override;
    void copy(const ScriptInputViewModelPropertyBase& object)
    {
        copyDataBindPathIds(object);
        CustomProperty::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case dataBindPathIdsPropertyKey:
                decodeDataBindPathIds(CoreBytesType::deserialize(reader));
                return true;
        }
        return CustomProperty::deserialize(propertyKey, reader);
    }

protected:
    virtual void dataBindPathIdsChanged() {}
};
} // namespace rive

#endif
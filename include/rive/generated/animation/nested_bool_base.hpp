#ifndef _RIVE_NESTED_BOOL_BASE_HPP_
#define _RIVE_NESTED_BOOL_BASE_HPP_
#include "rive/animation/nested_input.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
namespace rive
{
class NestedBoolBase : public NestedInput
{
protected:
    typedef NestedInput Super;

public:
    static const uint16_t typeKey = 123;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case NestedBoolBase::typeKey:
            case NestedInputBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t nestedValuePropertyKey = 238;

private:
    bool m_NestedValue = false;

public:
    virtual bool nestedValue() const { return m_NestedValue; }
    virtual void nestedValue(bool value) = 0;

    Core* clone() const override;
    void copy(const NestedBoolBase& object)
    {
        m_NestedValue = object.m_NestedValue;
        NestedInput::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case nestedValuePropertyKey:
                m_NestedValue = CoreBoolType::deserialize(reader);
                return true;
        }
        return NestedInput::deserialize(propertyKey, reader);
    }

protected:
    virtual void nestedValueChanged() {}
};
} // namespace rive

#endif
#ifndef _RIVE_NESTED_NUMBER_BASE_HPP_
#define _RIVE_NESTED_NUMBER_BASE_HPP_
#include "rive/animation/nested_input.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class NestedNumberBase : public NestedInput
{
protected:
    typedef NestedInput Super;

public:
    static const uint16_t typeKey = 124;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case NestedNumberBase::typeKey:
            case NestedInputBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t nestedValuePropertyKey = 239;

private:
    float m_NestedValue = 0.0f;

public:
    inline float nestedValue() const { return m_NestedValue; }
    void nestedValue(float value)
    {
        if (m_NestedValue == value)
        {
            return;
        }
        m_NestedValue = value;
        nestedValueChanged();
    }

    Core* clone() const override;
    void copy(const NestedNumberBase& object)
    {
        m_NestedValue = object.m_NestedValue;
        NestedInput::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case nestedValuePropertyKey:
                m_NestedValue = CoreDoubleType::deserialize(reader);
                return true;
        }
        return NestedInput::deserialize(propertyKey, reader);
    }

protected:
    virtual void nestedValueChanged() {}
};
} // namespace rive

#endif
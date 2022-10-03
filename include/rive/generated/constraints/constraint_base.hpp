#ifndef _RIVE_CONSTRAINT_BASE_HPP_
#define _RIVE_CONSTRAINT_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class ConstraintBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 79;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ConstraintBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t strengthPropertyKey = 172;

private:
    float m_Strength = 1.0f;

public:
    inline float strength() const { return m_Strength; }
    void strength(float value)
    {
        if (m_Strength == value)
        {
            return;
        }
        m_Strength = value;
        strengthChanged();
    }

    void copy(const ConstraintBase& object)
    {
        m_Strength = object.m_Strength;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case strengthPropertyKey:
                m_Strength = CoreDoubleType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void strengthChanged() {}
};
} // namespace rive

#endif
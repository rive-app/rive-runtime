#ifndef _RIVE_DASH_BASE_HPP_
#define _RIVE_DASH_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class DashBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 507;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DashBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t lengthPropertyKey = 692;
    static const uint16_t lengthIsPercentagePropertyKey = 693;

protected:
    float m_Length = 0.0f;
    bool m_LengthIsPercentage = false;

public:
    inline float length() const { return m_Length; }
    void length(float value)
    {
        if (m_Length == value)
        {
            return;
        }
        m_Length = value;
        lengthChanged();
    }

    inline bool lengthIsPercentage() const { return m_LengthIsPercentage; }
    void lengthIsPercentage(bool value)
    {
        if (m_LengthIsPercentage == value)
        {
            return;
        }
        m_LengthIsPercentage = value;
        lengthIsPercentageChanged();
    }

    Core* clone() const override;
    void copy(const DashBase& object)
    {
        m_Length = object.m_Length;
        m_LengthIsPercentage = object.m_LengthIsPercentage;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case lengthPropertyKey:
                m_Length = CoreDoubleType::deserialize(reader);
                return true;
            case lengthIsPercentagePropertyKey:
                m_LengthIsPercentage = CoreBoolType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void lengthChanged() {}
    virtual void lengthIsPercentageChanged() {}
};
} // namespace rive

#endif
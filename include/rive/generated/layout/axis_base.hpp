#ifndef _RIVE_AXIS_BASE_HPP_
#define _RIVE_AXIS_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class AxisBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 492;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case AxisBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t offsetPropertyKey = 675;
    static const uint16_t normalizedPropertyKey = 676;

protected:
    float m_Offset = 0.0f;
    bool m_Normalized = false;

public:
    inline float offset() const { return m_Offset; }
    void offset(float value)
    {
        if (m_Offset == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(offsetPropertyKey, &m_Offset, &value);
        m_Offset = value;
        RIVE_EDITOR_CHANGED(offsetChanged());
        notifyPropertyChanged(offsetPropertyKey);
    }

    inline bool normalized() const { return m_Normalized; }
    void normalized(bool value)
    {
        if (m_Normalized == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(normalizedPropertyKey, &m_Normalized, &value);
        m_Normalized = value;
        RIVE_EDITOR_CHANGED(normalizedChanged());
        notifyPropertyChanged(normalizedPropertyKey);
    }

    void copy(const AxisBase& object)
    {
        m_Offset = object.m_Offset;
        m_Normalized = object.m_Normalized;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case offsetPropertyKey:
                m_Offset = CoreDoubleType::deserialize(reader);
                return true;
            case normalizedPropertyKey:
                m_Normalized = CoreBoolType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void offsetChanged() {}
    virtual void normalizedChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/layout/axis_ext.inl"
#endif
};
} // namespace rive

#endif
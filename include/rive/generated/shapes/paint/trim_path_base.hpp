#ifndef _RIVE_TRIM_PATH_BASE_HPP_
#define _RIVE_TRIM_PATH_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class TrimPathBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 47;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TrimPathBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t startPropertyKey = 114;
    static const uint16_t endPropertyKey = 115;
    static const uint16_t offsetPropertyKey = 116;
    static const uint16_t modeValuePropertyKey = 117;

private:
    float m_Start = 0.0f;
    float m_End = 0.0f;
    float m_Offset = 0.0f;
    uint32_t m_ModeValue = 0;

public:
    inline float start() const { return m_Start; }
    void start(float value)
    {
        if (m_Start == value)
        {
            return;
        }
        m_Start = value;
        startChanged();
    }

    inline float end() const { return m_End; }
    void end(float value)
    {
        if (m_End == value)
        {
            return;
        }
        m_End = value;
        endChanged();
    }

    inline float offset() const { return m_Offset; }
    void offset(float value)
    {
        if (m_Offset == value)
        {
            return;
        }
        m_Offset = value;
        offsetChanged();
    }

    inline uint32_t modeValue() const { return m_ModeValue; }
    void modeValue(uint32_t value)
    {
        if (m_ModeValue == value)
        {
            return;
        }
        m_ModeValue = value;
        modeValueChanged();
    }

    Core* clone() const override;
    void copy(const TrimPathBase& object)
    {
        m_Start = object.m_Start;
        m_End = object.m_End;
        m_Offset = object.m_Offset;
        m_ModeValue = object.m_ModeValue;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case startPropertyKey:
                m_Start = CoreDoubleType::deserialize(reader);
                return true;
            case endPropertyKey:
                m_End = CoreDoubleType::deserialize(reader);
                return true;
            case offsetPropertyKey:
                m_Offset = CoreDoubleType::deserialize(reader);
                return true;
            case modeValuePropertyKey:
                m_ModeValue = CoreUintType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void startChanged() {}
    virtual void endChanged() {}
    virtual void offsetChanged() {}
    virtual void modeValueChanged() {}
};
} // namespace rive

#endif
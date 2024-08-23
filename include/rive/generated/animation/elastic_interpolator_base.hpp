#ifndef _RIVE_ELASTIC_INTERPOLATOR_BASE_HPP_
#define _RIVE_ELASTIC_INTERPOLATOR_BASE_HPP_
#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class ElasticInterpolatorBase : public KeyFrameInterpolator
{
protected:
    typedef KeyFrameInterpolator Super;

public:
    static const uint16_t typeKey = 174;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ElasticInterpolatorBase::typeKey:
            case KeyFrameInterpolatorBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t easingValuePropertyKey = 405;
    static const uint16_t amplitudePropertyKey = 406;
    static const uint16_t periodPropertyKey = 407;

private:
    uint32_t m_EasingValue = 1;
    float m_Amplitude = 1.0f;
    float m_Period = 1.0f;

public:
    inline uint32_t easingValue() const { return m_EasingValue; }
    void easingValue(uint32_t value)
    {
        if (m_EasingValue == value)
        {
            return;
        }
        m_EasingValue = value;
        easingValueChanged();
    }

    inline float amplitude() const { return m_Amplitude; }
    void amplitude(float value)
    {
        if (m_Amplitude == value)
        {
            return;
        }
        m_Amplitude = value;
        amplitudeChanged();
    }

    inline float period() const { return m_Period; }
    void period(float value)
    {
        if (m_Period == value)
        {
            return;
        }
        m_Period = value;
        periodChanged();
    }

    Core* clone() const override;
    void copy(const ElasticInterpolatorBase& object)
    {
        m_EasingValue = object.m_EasingValue;
        m_Amplitude = object.m_Amplitude;
        m_Period = object.m_Period;
        KeyFrameInterpolator::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case easingValuePropertyKey:
                m_EasingValue = CoreUintType::deserialize(reader);
                return true;
            case amplitudePropertyKey:
                m_Amplitude = CoreDoubleType::deserialize(reader);
                return true;
            case periodPropertyKey:
                m_Period = CoreDoubleType::deserialize(reader);
                return true;
        }
        return KeyFrameInterpolator::deserialize(propertyKey, reader);
    }

protected:
    virtual void easingValueChanged() {}
    virtual void amplitudeChanged() {}
    virtual void periodChanged() {}
};
} // namespace rive

#endif
#ifndef _RIVE_CUBIC_INTERPOLATOR_BASE_HPP_
#define _RIVE_CUBIC_INTERPOLATOR_BASE_HPP_
#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class CubicInterpolatorBase : public KeyFrameInterpolator
{
protected:
    typedef KeyFrameInterpolator Super;

public:
    static const uint16_t typeKey = 139;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case CubicInterpolatorBase::typeKey:
            case KeyFrameInterpolatorBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t x1PropertyKey = 63;
    static const uint16_t y1PropertyKey = 64;
    static const uint16_t x2PropertyKey = 65;
    static const uint16_t y2PropertyKey = 66;

protected:
    float m_X1 = 0.42f;
    float m_Y1 = 0.0f;
    float m_X2 = 0.58f;
    float m_Y2 = 1.0f;

public:
    inline float x1() const { return m_X1; }
    void x1(float value)
    {
        if (m_X1 == value)
        {
            return;
        }
        m_X1 = value;
        x1Changed();
    }

    inline float y1() const { return m_Y1; }
    void y1(float value)
    {
        if (m_Y1 == value)
        {
            return;
        }
        m_Y1 = value;
        y1Changed();
    }

    inline float x2() const { return m_X2; }
    void x2(float value)
    {
        if (m_X2 == value)
        {
            return;
        }
        m_X2 = value;
        x2Changed();
    }

    inline float y2() const { return m_Y2; }
    void y2(float value)
    {
        if (m_Y2 == value)
        {
            return;
        }
        m_Y2 = value;
        y2Changed();
    }

    void copy(const CubicInterpolatorBase& object)
    {
        m_X1 = object.m_X1;
        m_Y1 = object.m_Y1;
        m_X2 = object.m_X2;
        m_Y2 = object.m_Y2;
        KeyFrameInterpolator::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case x1PropertyKey:
                m_X1 = CoreDoubleType::deserialize(reader);
                return true;
            case y1PropertyKey:
                m_Y1 = CoreDoubleType::deserialize(reader);
                return true;
            case x2PropertyKey:
                m_X2 = CoreDoubleType::deserialize(reader);
                return true;
            case y2PropertyKey:
                m_Y2 = CoreDoubleType::deserialize(reader);
                return true;
        }
        return KeyFrameInterpolator::deserialize(propertyKey, reader);
    }

protected:
    virtual void x1Changed() {}
    virtual void y1Changed() {}
    virtual void x2Changed() {}
    virtual void y2Changed() {}
};
} // namespace rive

#endif
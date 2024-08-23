#ifndef _RIVE_BLEND_ANIMATION1_DBASE_HPP_
#define _RIVE_BLEND_ANIMATION1_DBASE_HPP_
#include "rive/animation/blend_animation.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class BlendAnimation1DBase : public BlendAnimation
{
protected:
    typedef BlendAnimation Super;

public:
    static const uint16_t typeKey = 75;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case BlendAnimation1DBase::typeKey:
            case BlendAnimationBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t valuePropertyKey = 166;

private:
    float m_Value = 0.0f;

public:
    inline float value() const { return m_Value; }
    void value(float value)
    {
        if (m_Value == value)
        {
            return;
        }
        m_Value = value;
        valueChanged();
    }

    Core* clone() const override;
    void copy(const BlendAnimation1DBase& object)
    {
        m_Value = object.m_Value;
        BlendAnimation::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case valuePropertyKey:
                m_Value = CoreDoubleType::deserialize(reader);
                return true;
        }
        return BlendAnimation::deserialize(propertyKey, reader);
    }

protected:
    virtual void valueChanged() {}
};
} // namespace rive

#endif
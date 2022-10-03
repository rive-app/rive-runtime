#ifndef _RIVE_NESTED_SIMPLE_ANIMATION_BASE_HPP_
#define _RIVE_NESTED_SIMPLE_ANIMATION_BASE_HPP_
#include "rive/animation/nested_linear_animation.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class NestedSimpleAnimationBase : public NestedLinearAnimation
{
protected:
    typedef NestedLinearAnimation Super;

public:
    static const uint16_t typeKey = 96;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case NestedSimpleAnimationBase::typeKey:
            case NestedLinearAnimationBase::typeKey:
            case NestedAnimationBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t speedPropertyKey = 199;
    static const uint16_t isPlayingPropertyKey = 201;

private:
    float m_Speed = 1.0f;
    bool m_IsPlaying = false;

public:
    inline float speed() const { return m_Speed; }
    void speed(float value)
    {
        if (m_Speed == value)
        {
            return;
        }
        m_Speed = value;
        speedChanged();
    }

    inline bool isPlaying() const { return m_IsPlaying; }
    void isPlaying(bool value)
    {
        if (m_IsPlaying == value)
        {
            return;
        }
        m_IsPlaying = value;
        isPlayingChanged();
    }

    Core* clone() const override;
    void copy(const NestedSimpleAnimationBase& object)
    {
        m_Speed = object.m_Speed;
        m_IsPlaying = object.m_IsPlaying;
        NestedLinearAnimation::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case speedPropertyKey:
                m_Speed = CoreDoubleType::deserialize(reader);
                return true;
            case isPlayingPropertyKey:
                m_IsPlaying = CoreBoolType::deserialize(reader);
                return true;
        }
        return NestedLinearAnimation::deserialize(propertyKey, reader);
    }

protected:
    virtual void speedChanged() {}
    virtual void isPlayingChanged() {}
};
} // namespace rive

#endif
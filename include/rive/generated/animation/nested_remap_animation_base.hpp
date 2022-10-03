#ifndef _RIVE_NESTED_REMAP_ANIMATION_BASE_HPP_
#define _RIVE_NESTED_REMAP_ANIMATION_BASE_HPP_
#include "rive/animation/nested_linear_animation.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class NestedRemapAnimationBase : public NestedLinearAnimation
{
protected:
    typedef NestedLinearAnimation Super;

public:
    static const uint16_t typeKey = 98;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case NestedRemapAnimationBase::typeKey:
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

    static const uint16_t timePropertyKey = 202;

private:
    float m_Time = 0.0f;

public:
    inline float time() const { return m_Time; }
    void time(float value)
    {
        if (m_Time == value)
        {
            return;
        }
        m_Time = value;
        timeChanged();
    }

    Core* clone() const override;
    void copy(const NestedRemapAnimationBase& object)
    {
        m_Time = object.m_Time;
        NestedLinearAnimation::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case timePropertyKey:
                m_Time = CoreDoubleType::deserialize(reader);
                return true;
        }
        return NestedLinearAnimation::deserialize(propertyKey, reader);
    }

protected:
    virtual void timeChanged() {}
};
} // namespace rive

#endif
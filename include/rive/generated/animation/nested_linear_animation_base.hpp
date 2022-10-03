#ifndef _RIVE_NESTED_LINEAR_ANIMATION_BASE_HPP_
#define _RIVE_NESTED_LINEAR_ANIMATION_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/nested_animation.hpp"
namespace rive
{
class NestedLinearAnimationBase : public NestedAnimation
{
protected:
    typedef NestedAnimation Super;

public:
    static const uint16_t typeKey = 97;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
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

    static const uint16_t mixPropertyKey = 200;

private:
    float m_Mix = 1.0f;

public:
    inline float mix() const { return m_Mix; }
    void mix(float value)
    {
        if (m_Mix == value)
        {
            return;
        }
        m_Mix = value;
        mixChanged();
    }

    void copy(const NestedLinearAnimationBase& object)
    {
        m_Mix = object.m_Mix;
        NestedAnimation::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case mixPropertyKey:
                m_Mix = CoreDoubleType::deserialize(reader);
                return true;
        }
        return NestedAnimation::deserialize(propertyKey, reader);
    }

protected:
    virtual void mixChanged() {}
};
} // namespace rive

#endif
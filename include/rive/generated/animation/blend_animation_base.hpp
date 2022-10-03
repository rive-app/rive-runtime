#ifndef _RIVE_BLEND_ANIMATION_BASE_HPP_
#define _RIVE_BLEND_ANIMATION_BASE_HPP_
#include "rive/core.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class BlendAnimationBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 74;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case BlendAnimationBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t animationIdPropertyKey = 165;

private:
    uint32_t m_AnimationId = -1;

public:
    inline uint32_t animationId() const { return m_AnimationId; }
    void animationId(uint32_t value)
    {
        if (m_AnimationId == value)
        {
            return;
        }
        m_AnimationId = value;
        animationIdChanged();
    }

    void copy(const BlendAnimationBase& object) { m_AnimationId = object.m_AnimationId; }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case animationIdPropertyKey:
                m_AnimationId = CoreUintType::deserialize(reader);
                return true;
        }
        return false;
    }

protected:
    virtual void animationIdChanged() {}
};
} // namespace rive

#endif
#ifndef _RIVE_BLEND_ANIMATION_BASE_HPP_
#define _RIVE_BLEND_ANIMATION_BASE_HPP_
#include "rive/core.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class BlendAnimationBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 74;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
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

protected:
    Id m_AnimationId = kEmptyId;

public:
    inline Id animationId() const { return m_AnimationId; }
    void animationId(Id value)
    {
        if (m_AnimationId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(animationIdPropertyKey, &m_AnimationId, &value);
        m_AnimationId = value;
        RIVE_EDITOR_CHANGED(animationIdChanged());
        notifyPropertyChanged(animationIdPropertyKey);
    }

    void copy(const BlendAnimationBase& object)
    {
        m_AnimationId = object.m_AnimationId;
        RIVE_EDITOR_COPY(object);
        RIVE_EDITOR_COPY_VALIDATED(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case animationIdPropertyKey:
                m_AnimationId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return false;
    }

protected:
    virtual void animationIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/blend_animation_ext.inl"
#endif
};
} // namespace rive

#endif
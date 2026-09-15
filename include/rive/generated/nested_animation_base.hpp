#ifndef _RIVE_NESTED_ANIMATION_BASE_HPP_
#define _RIVE_NESTED_ANIMATION_BASE_HPP_
#include "rive/container_component.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
namespace rive
{
class NestedAnimationBase : public ContainerComponent
{
protected:
    typedef ContainerComponent Super;

public:
    static const uint16_t typeKey = 93;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case NestedAnimationBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t animationIdPropertyKey = 198;

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

    void copy(const NestedAnimationBase& object)
    {
        m_AnimationId = object.m_AnimationId;
        ContainerComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case animationIdPropertyKey:
                m_AnimationId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        return ContainerComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void animationIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/nested_animation_ext.inl"
#endif
};
} // namespace rive

#endif
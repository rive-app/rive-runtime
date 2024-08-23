#ifndef _RIVE_NESTED_ANIMATION_BASE_HPP_
#define _RIVE_NESTED_ANIMATION_BASE_HPP_
#include "rive/container_component.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class NestedAnimationBase : public ContainerComponent
{
protected:
    typedef ContainerComponent Super;

public:
    static const uint16_t typeKey = 93;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
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
                m_AnimationId = CoreUintType::deserialize(reader);
                return true;
        }
        return ContainerComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void animationIdChanged() {}
};
} // namespace rive

#endif
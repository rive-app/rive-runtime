#ifndef _RIVE_GROUP_EFFECT_BASE_HPP_
#define _RIVE_GROUP_EFFECT_BASE_HPP_
#include "rive/container_component.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class GroupEffectBase : public ContainerComponent
{
protected:
    typedef ContainerComponent Super;

public:
    static const uint16_t typeKey = 645;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case GroupEffectBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

public:
    Core* clone() const override;
    void copy(const GroupEffectBase& object)
    {
        RIVE_EDITOR_COPY(object);
        ContainerComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return ContainerComponent::deserialize(propertyKey, reader);
    }
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/shapes/paint/group_effect_ext.inl"
#endif
};
} // namespace rive

#endif
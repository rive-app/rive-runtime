#ifndef _RIVE_WORLD_TRANSFORM_COMPONENT_BASE_HPP_
#define _RIVE_WORLD_TRANSFORM_COMPONENT_BASE_HPP_
#include "rive/container_component.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class WorldTransformComponentBase : public ContainerComponent
{
protected:
    typedef ContainerComponent Super;

public:
    static const uint16_t typeKey = 91;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case WorldTransformComponentBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t opacityPropertyKey = 18;

protected:
    float m_Opacity = 1.0f;

public:
    inline float opacity() const { return m_Opacity; }
    void opacity(float value)
    {
        if (m_Opacity == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(opacityPropertyKey, &m_Opacity, &value);
        m_Opacity = value;
        RIVE_EDITOR_CHANGED(opacityChanged());
        notifyPropertyChanged(opacityPropertyKey);
    }

    void copy(const WorldTransformComponentBase& object)
    {
        m_Opacity = object.m_Opacity;
        ContainerComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case opacityPropertyKey:
                m_Opacity = CoreDoubleType::deserialize(reader);
                return true;
        }
        return ContainerComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void opacityChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/world_transform_component_ext.inl"
#endif
};
} // namespace rive

#endif
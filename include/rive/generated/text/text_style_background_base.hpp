#ifndef _RIVE_TEXT_STYLE_BACKGROUND_BASE_HPP_
#define _RIVE_TEXT_STYLE_BACKGROUND_BASE_HPP_
#include "rive/container_component.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class TextStyleBackgroundBase : public ContainerComponent
{
protected:
    typedef ContainerComponent Super;

public:
    static const uint16_t typeKey = 1069;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TextStyleBackgroundBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t cornerRadiusPropertyKey = 1071;

protected:
    float m_CornerRadius = 0.0f;

public:
    inline float cornerRadius() const { return m_CornerRadius; }
    void cornerRadius(float value)
    {
        if (m_CornerRadius == value)
        {
            return;
        }
        m_CornerRadius = value;
        cornerRadiusChanged();
        notifyPropertyChanged(cornerRadiusPropertyKey);
    }

    Core* clone() const override;
    void copy(const TextStyleBackgroundBase& object)
    {
        m_CornerRadius = object.m_CornerRadius;
        ContainerComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case cornerRadiusPropertyKey:
                m_CornerRadius = CoreDoubleType::deserialize(reader);
                return true;
        }
        return ContainerComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void cornerRadiusChanged() {}
};
} // namespace rive

#endif
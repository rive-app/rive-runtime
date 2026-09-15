#ifndef _RIVE_SHAPE_PAINT_BASE_HPP_
#define _RIVE_SHAPE_PAINT_BASE_HPP_
#include "rive/container_component.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class ShapePaintBase : public ContainerComponent
{
protected:
    typedef ContainerComponent Super;

public:
    static const uint16_t typeKey = 21;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ShapePaintBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t isVisiblePropertyKey = 41;
    static const uint16_t blendModeValuePropertyKey = 747;

protected:
    bool m_IsVisible = true;
    uint32_t m_BlendModeValue = 127;

public:
    virtual bool isVisible() const { return m_IsVisible; }
    void isVisible(bool value)
    {
        if (m_IsVisible == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(isVisiblePropertyKey, &m_IsVisible, &value);
        m_IsVisible = value;
        RIVE_EDITOR_CHANGED(isVisibleChanged());
        notifyPropertyChanged(isVisiblePropertyKey);
    }

    inline uint32_t blendModeValue() const { return m_BlendModeValue; }
    void blendModeValue(uint32_t value)
    {
        if (m_BlendModeValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(blendModeValuePropertyKey,
                             &m_BlendModeValue,
                             &value);
        m_BlendModeValue = value;
        RIVE_EDITOR_CHANGED(blendModeValueChanged());
        notifyPropertyChanged(blendModeValuePropertyKey);
    }

    void copy(const ShapePaintBase& object)
    {
        m_IsVisible = object.m_IsVisible;
        m_BlendModeValue = object.m_BlendModeValue;
        RIVE_EDITOR_COPY(object);
        ContainerComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case isVisiblePropertyKey:
                m_IsVisible = CoreBoolType::deserialize(reader);
                return true;
            case blendModeValuePropertyKey:
                m_BlendModeValue = CoreUintType::deserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return ContainerComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void isVisibleChanged() {}
    virtual void blendModeValueChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/shapes/paint/shape_paint_ext.inl"
#endif
};
} // namespace rive

#endif
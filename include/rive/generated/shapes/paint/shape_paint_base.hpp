#ifndef _RIVE_SHAPE_PAINT_BASE_HPP_
#define _RIVE_SHAPE_PAINT_BASE_HPP_
#include "rive/container_component.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
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
        m_IsVisible = value;
        isVisibleChanged();
    }

    inline uint32_t blendModeValue() const { return m_BlendModeValue; }
    void blendModeValue(uint32_t value)
    {
        if (m_BlendModeValue == value)
        {
            return;
        }
        m_BlendModeValue = value;
        blendModeValueChanged();
    }

    void copy(const ShapePaintBase& object)
    {
        m_IsVisible = object.m_IsVisible;
        m_BlendModeValue = object.m_BlendModeValue;
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
        return ContainerComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void isVisibleChanged() {}
    virtual void blendModeValueChanged() {}
};
} // namespace rive

#endif
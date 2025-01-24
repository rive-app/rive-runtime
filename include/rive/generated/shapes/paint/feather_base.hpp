#ifndef _RIVE_FEATHER_BASE_HPP_
#define _RIVE_FEATHER_BASE_HPP_
#include "rive/container_component.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class FeatherBase : public ContainerComponent
{
protected:
    typedef ContainerComponent Super;

public:
    static const uint16_t typeKey = 533;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case FeatherBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t spaceValuePropertyKey = 748;
    static const uint16_t strengthPropertyKey = 749;
    static const uint16_t offsetXPropertyKey = 750;
    static const uint16_t offsetYPropertyKey = 751;
    static const uint16_t innerPropertyKey = 752;

protected:
    uint32_t m_SpaceValue = 0;
    float m_Strength = 12.0f;
    float m_OffsetX = 0.0f;
    float m_OffsetY = 0.0f;
    bool m_Inner = false;

public:
    inline uint32_t spaceValue() const { return m_SpaceValue; }
    void spaceValue(uint32_t value)
    {
        if (m_SpaceValue == value)
        {
            return;
        }
        m_SpaceValue = value;
        spaceValueChanged();
    }

    inline float strength() const { return m_Strength; }
    void strength(float value)
    {
        if (m_Strength == value)
        {
            return;
        }
        m_Strength = value;
        strengthChanged();
    }

    inline float offsetX() const { return m_OffsetX; }
    void offsetX(float value)
    {
        if (m_OffsetX == value)
        {
            return;
        }
        m_OffsetX = value;
        offsetXChanged();
    }

    inline float offsetY() const { return m_OffsetY; }
    void offsetY(float value)
    {
        if (m_OffsetY == value)
        {
            return;
        }
        m_OffsetY = value;
        offsetYChanged();
    }

    inline bool inner() const { return m_Inner; }
    void inner(bool value)
    {
        if (m_Inner == value)
        {
            return;
        }
        m_Inner = value;
        innerChanged();
    }

    Core* clone() const override;
    void copy(const FeatherBase& object)
    {
        m_SpaceValue = object.m_SpaceValue;
        m_Strength = object.m_Strength;
        m_OffsetX = object.m_OffsetX;
        m_OffsetY = object.m_OffsetY;
        m_Inner = object.m_Inner;
        ContainerComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case spaceValuePropertyKey:
                m_SpaceValue = CoreUintType::deserialize(reader);
                return true;
            case strengthPropertyKey:
                m_Strength = CoreDoubleType::deserialize(reader);
                return true;
            case offsetXPropertyKey:
                m_OffsetX = CoreDoubleType::deserialize(reader);
                return true;
            case offsetYPropertyKey:
                m_OffsetY = CoreDoubleType::deserialize(reader);
                return true;
            case innerPropertyKey:
                m_Inner = CoreBoolType::deserialize(reader);
                return true;
        }
        return ContainerComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void spaceValueChanged() {}
    virtual void strengthChanged() {}
    virtual void offsetXChanged() {}
    virtual void offsetYChanged() {}
    virtual void innerChanged() {}
};
} // namespace rive

#endif
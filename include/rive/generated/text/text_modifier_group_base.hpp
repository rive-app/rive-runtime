#ifndef _RIVE_TEXT_MODIFIER_GROUP_BASE_HPP_
#define _RIVE_TEXT_MODIFIER_GROUP_BASE_HPP_
#include "rive/container_component.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class TextModifierGroupBase : public ContainerComponent
{
protected:
    typedef ContainerComponent Super;

public:
    static const uint16_t typeKey = 159;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TextModifierGroupBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t modifierFlagsPropertyKey = 335;
    static const uint16_t originXPropertyKey = 328;
    static const uint16_t originYPropertyKey = 329;
    static const uint16_t opacityPropertyKey = 324;
    static const uint16_t xPropertyKey = 322;
    static const uint16_t yPropertyKey = 323;
    static const uint16_t rotationPropertyKey = 332;
    static const uint16_t scaleXPropertyKey = 330;
    static const uint16_t scaleYPropertyKey = 331;

private:
    uint32_t m_ModifierFlags = 0;
    float m_OriginX = 0.0f;
    float m_OriginY = 0.0f;
    float m_Opacity = 1.0f;
    float m_X = 0.0f;
    float m_Y = 0.0f;
    float m_Rotation = 0.0f;
    float m_ScaleX = 1.0f;
    float m_ScaleY = 1.0f;

public:
    inline uint32_t modifierFlags() const { return m_ModifierFlags; }
    void modifierFlags(uint32_t value)
    {
        if (m_ModifierFlags == value)
        {
            return;
        }
        m_ModifierFlags = value;
        modifierFlagsChanged();
    }

    inline float originX() const { return m_OriginX; }
    void originX(float value)
    {
        if (m_OriginX == value)
        {
            return;
        }
        m_OriginX = value;
        originXChanged();
    }

    inline float originY() const { return m_OriginY; }
    void originY(float value)
    {
        if (m_OriginY == value)
        {
            return;
        }
        m_OriginY = value;
        originYChanged();
    }

    inline float opacity() const { return m_Opacity; }
    void opacity(float value)
    {
        if (m_Opacity == value)
        {
            return;
        }
        m_Opacity = value;
        opacityChanged();
    }

    inline float x() const { return m_X; }
    void x(float value)
    {
        if (m_X == value)
        {
            return;
        }
        m_X = value;
        xChanged();
    }

    inline float y() const { return m_Y; }
    void y(float value)
    {
        if (m_Y == value)
        {
            return;
        }
        m_Y = value;
        yChanged();
    }

    inline float rotation() const { return m_Rotation; }
    void rotation(float value)
    {
        if (m_Rotation == value)
        {
            return;
        }
        m_Rotation = value;
        rotationChanged();
    }

    inline float scaleX() const { return m_ScaleX; }
    void scaleX(float value)
    {
        if (m_ScaleX == value)
        {
            return;
        }
        m_ScaleX = value;
        scaleXChanged();
    }

    inline float scaleY() const { return m_ScaleY; }
    void scaleY(float value)
    {
        if (m_ScaleY == value)
        {
            return;
        }
        m_ScaleY = value;
        scaleYChanged();
    }

    Core* clone() const override;
    void copy(const TextModifierGroupBase& object)
    {
        m_ModifierFlags = object.m_ModifierFlags;
        m_OriginX = object.m_OriginX;
        m_OriginY = object.m_OriginY;
        m_Opacity = object.m_Opacity;
        m_X = object.m_X;
        m_Y = object.m_Y;
        m_Rotation = object.m_Rotation;
        m_ScaleX = object.m_ScaleX;
        m_ScaleY = object.m_ScaleY;
        ContainerComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case modifierFlagsPropertyKey:
                m_ModifierFlags = CoreUintType::deserialize(reader);
                return true;
            case originXPropertyKey:
                m_OriginX = CoreDoubleType::deserialize(reader);
                return true;
            case originYPropertyKey:
                m_OriginY = CoreDoubleType::deserialize(reader);
                return true;
            case opacityPropertyKey:
                m_Opacity = CoreDoubleType::deserialize(reader);
                return true;
            case xPropertyKey:
                m_X = CoreDoubleType::deserialize(reader);
                return true;
            case yPropertyKey:
                m_Y = CoreDoubleType::deserialize(reader);
                return true;
            case rotationPropertyKey:
                m_Rotation = CoreDoubleType::deserialize(reader);
                return true;
            case scaleXPropertyKey:
                m_ScaleX = CoreDoubleType::deserialize(reader);
                return true;
            case scaleYPropertyKey:
                m_ScaleY = CoreDoubleType::deserialize(reader);
                return true;
        }
        return ContainerComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void modifierFlagsChanged() {}
    virtual void originXChanged() {}
    virtual void originYChanged() {}
    virtual void opacityChanged() {}
    virtual void xChanged() {}
    virtual void yChanged() {}
    virtual void rotationChanged() {}
    virtual void scaleXChanged() {}
    virtual void scaleYChanged() {}
};
} // namespace rive

#endif
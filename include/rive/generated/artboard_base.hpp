#ifndef _RIVE_ARTBOARD_BASE_HPP_
#define _RIVE_ARTBOARD_BASE_HPP_
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/world_transform_component.hpp"
namespace rive
{
class ArtboardBase : public WorldTransformComponent
{
protected:
    typedef WorldTransformComponent Super;

public:
    static const uint16_t typeKey = 1;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ArtboardBase::typeKey:
            case WorldTransformComponentBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t clipPropertyKey = 196;
    static const uint16_t widthPropertyKey = 7;
    static const uint16_t heightPropertyKey = 8;
    static const uint16_t xPropertyKey = 9;
    static const uint16_t yPropertyKey = 10;
    static const uint16_t originXPropertyKey = 11;
    static const uint16_t originYPropertyKey = 12;
    static const uint16_t defaultStateMachineIdPropertyKey = 236;

private:
    bool m_Clip = true;
    float m_Width = 0.0f;
    float m_Height = 0.0f;
    float m_X = 0.0f;
    float m_Y = 0.0f;
    float m_OriginX = 0.0f;
    float m_OriginY = 0.0f;
    uint32_t m_DefaultStateMachineId = -1;

public:
    inline bool clip() const { return m_Clip; }
    void clip(bool value)
    {
        if (m_Clip == value)
        {
            return;
        }
        m_Clip = value;
        clipChanged();
    }

    inline float width() const { return m_Width; }
    void width(float value)
    {
        if (m_Width == value)
        {
            return;
        }
        m_Width = value;
        widthChanged();
    }

    inline float height() const { return m_Height; }
    void height(float value)
    {
        if (m_Height == value)
        {
            return;
        }
        m_Height = value;
        heightChanged();
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

    inline uint32_t defaultStateMachineId() const { return m_DefaultStateMachineId; }
    void defaultStateMachineId(uint32_t value)
    {
        if (m_DefaultStateMachineId == value)
        {
            return;
        }
        m_DefaultStateMachineId = value;
        defaultStateMachineIdChanged();
    }

    Core* clone() const override;
    void copy(const ArtboardBase& object)
    {
        m_Clip = object.m_Clip;
        m_Width = object.m_Width;
        m_Height = object.m_Height;
        m_X = object.m_X;
        m_Y = object.m_Y;
        m_OriginX = object.m_OriginX;
        m_OriginY = object.m_OriginY;
        m_DefaultStateMachineId = object.m_DefaultStateMachineId;
        WorldTransformComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case clipPropertyKey:
                m_Clip = CoreBoolType::deserialize(reader);
                return true;
            case widthPropertyKey:
                m_Width = CoreDoubleType::deserialize(reader);
                return true;
            case heightPropertyKey:
                m_Height = CoreDoubleType::deserialize(reader);
                return true;
            case xPropertyKey:
                m_X = CoreDoubleType::deserialize(reader);
                return true;
            case yPropertyKey:
                m_Y = CoreDoubleType::deserialize(reader);
                return true;
            case originXPropertyKey:
                m_OriginX = CoreDoubleType::deserialize(reader);
                return true;
            case originYPropertyKey:
                m_OriginY = CoreDoubleType::deserialize(reader);
                return true;
            case defaultStateMachineIdPropertyKey:
                m_DefaultStateMachineId = CoreUintType::deserialize(reader);
                return true;
        }
        return WorldTransformComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void clipChanged() {}
    virtual void widthChanged() {}
    virtual void heightChanged() {}
    virtual void xChanged() {}
    virtual void yChanged() {}
    virtual void originXChanged() {}
    virtual void originYChanged() {}
    virtual void defaultStateMachineIdChanged() {}
};
} // namespace rive

#endif
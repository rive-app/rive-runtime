#ifndef _RIVE_JOYSTICK_BASE_HPP_
#define _RIVE_JOYSTICK_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class JoystickBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 148;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case JoystickBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t xPropertyKey = 299;
    static const uint16_t yPropertyKey = 300;
    static const uint16_t posXPropertyKey = 303;
    static const uint16_t posYPropertyKey = 304;
    static const uint16_t originXPropertyKey = 307;
    static const uint16_t originYPropertyKey = 308;
    static const uint16_t widthPropertyKey = 305;
    static const uint16_t heightPropertyKey = 306;
    static const uint16_t xIdPropertyKey = 301;
    static const uint16_t yIdPropertyKey = 302;
    static const uint16_t joystickFlagsPropertyKey = 312;
    static const uint16_t handleSourceIdPropertyKey = 313;

private:
    float m_X = 0.0f;
    float m_Y = 0.0f;
    float m_PosX = 0.0f;
    float m_PosY = 0.0f;
    float m_OriginX = 0.5f;
    float m_OriginY = 0.5f;
    float m_Width = 100.0f;
    float m_Height = 100.0f;
    uint32_t m_XId = -1;
    uint32_t m_YId = -1;
    uint32_t m_JoystickFlags = 0;
    uint32_t m_HandleSourceId = -1;

public:
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

    inline float posX() const { return m_PosX; }
    void posX(float value)
    {
        if (m_PosX == value)
        {
            return;
        }
        m_PosX = value;
        posXChanged();
    }

    inline float posY() const { return m_PosY; }
    void posY(float value)
    {
        if (m_PosY == value)
        {
            return;
        }
        m_PosY = value;
        posYChanged();
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

    inline uint32_t xId() const { return m_XId; }
    void xId(uint32_t value)
    {
        if (m_XId == value)
        {
            return;
        }
        m_XId = value;
        xIdChanged();
    }

    inline uint32_t yId() const { return m_YId; }
    void yId(uint32_t value)
    {
        if (m_YId == value)
        {
            return;
        }
        m_YId = value;
        yIdChanged();
    }

    inline uint32_t joystickFlags() const { return m_JoystickFlags; }
    void joystickFlags(uint32_t value)
    {
        if (m_JoystickFlags == value)
        {
            return;
        }
        m_JoystickFlags = value;
        joystickFlagsChanged();
    }

    inline uint32_t handleSourceId() const { return m_HandleSourceId; }
    void handleSourceId(uint32_t value)
    {
        if (m_HandleSourceId == value)
        {
            return;
        }
        m_HandleSourceId = value;
        handleSourceIdChanged();
    }

    Core* clone() const override;
    void copy(const JoystickBase& object)
    {
        m_X = object.m_X;
        m_Y = object.m_Y;
        m_PosX = object.m_PosX;
        m_PosY = object.m_PosY;
        m_OriginX = object.m_OriginX;
        m_OriginY = object.m_OriginY;
        m_Width = object.m_Width;
        m_Height = object.m_Height;
        m_XId = object.m_XId;
        m_YId = object.m_YId;
        m_JoystickFlags = object.m_JoystickFlags;
        m_HandleSourceId = object.m_HandleSourceId;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case xPropertyKey:
                m_X = CoreDoubleType::deserialize(reader);
                return true;
            case yPropertyKey:
                m_Y = CoreDoubleType::deserialize(reader);
                return true;
            case posXPropertyKey:
                m_PosX = CoreDoubleType::deserialize(reader);
                return true;
            case posYPropertyKey:
                m_PosY = CoreDoubleType::deserialize(reader);
                return true;
            case originXPropertyKey:
                m_OriginX = CoreDoubleType::deserialize(reader);
                return true;
            case originYPropertyKey:
                m_OriginY = CoreDoubleType::deserialize(reader);
                return true;
            case widthPropertyKey:
                m_Width = CoreDoubleType::deserialize(reader);
                return true;
            case heightPropertyKey:
                m_Height = CoreDoubleType::deserialize(reader);
                return true;
            case xIdPropertyKey:
                m_XId = CoreUintType::deserialize(reader);
                return true;
            case yIdPropertyKey:
                m_YId = CoreUintType::deserialize(reader);
                return true;
            case joystickFlagsPropertyKey:
                m_JoystickFlags = CoreUintType::deserialize(reader);
                return true;
            case handleSourceIdPropertyKey:
                m_HandleSourceId = CoreUintType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void xChanged() {}
    virtual void yChanged() {}
    virtual void posXChanged() {}
    virtual void posYChanged() {}
    virtual void originXChanged() {}
    virtual void originYChanged() {}
    virtual void widthChanged() {}
    virtual void heightChanged() {}
    virtual void xIdChanged() {}
    virtual void yIdChanged() {}
    virtual void joystickFlagsChanged() {}
    virtual void handleSourceIdChanged() {}
};
} // namespace rive

#endif
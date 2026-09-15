#ifndef _RIVE_JOYSTICK_BASE_HPP_
#define _RIVE_JOYSTICK_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/core/id.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class JoystickBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 148;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
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

protected:
    float m_X = 0.0f;
    float m_Y = 0.0f;
    float m_PosX = 0.0f;
    float m_PosY = 0.0f;
    float m_OriginX = 0.5f;
    float m_OriginY = 0.5f;
    float m_Width = 100.0f;
    float m_Height = 100.0f;
    Id m_XId = kEmptyId;
    Id m_YId = kEmptyId;
    uint32_t m_JoystickFlags = 0;
    Id m_HandleSourceId = kEmptyId;

public:
    inline float x() const { return m_X; }
    void x(float value)
    {
        if (m_X == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(xPropertyKey, &m_X, &value);
        m_X = value;
        RIVE_EDITOR_CHANGED(xChanged());
        notifyPropertyChanged(xPropertyKey);
    }

    inline float y() const { return m_Y; }
    void y(float value)
    {
        if (m_Y == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(yPropertyKey, &m_Y, &value);
        m_Y = value;
        RIVE_EDITOR_CHANGED(yChanged());
        notifyPropertyChanged(yPropertyKey);
    }

    inline float posX() const { return m_PosX; }
    void posX(float value)
    {
        if (m_PosX == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(posXPropertyKey, &m_PosX, &value);
        m_PosX = value;
        RIVE_EDITOR_CHANGED(posXChanged());
        notifyPropertyChanged(posXPropertyKey);
    }

    inline float posY() const { return m_PosY; }
    void posY(float value)
    {
        if (m_PosY == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(posYPropertyKey, &m_PosY, &value);
        m_PosY = value;
        RIVE_EDITOR_CHANGED(posYChanged());
        notifyPropertyChanged(posYPropertyKey);
    }

    inline float originX() const { return m_OriginX; }
    void originX(float value)
    {
        if (m_OriginX == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(originXPropertyKey, &m_OriginX, &value);
        m_OriginX = value;
        RIVE_EDITOR_CHANGED(originXChanged());
        notifyPropertyChanged(originXPropertyKey);
    }

    inline float originY() const { return m_OriginY; }
    void originY(float value)
    {
        if (m_OriginY == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(originYPropertyKey, &m_OriginY, &value);
        m_OriginY = value;
        RIVE_EDITOR_CHANGED(originYChanged());
        notifyPropertyChanged(originYPropertyKey);
    }

    inline float width() const { return m_Width; }
    void width(float value)
    {
        if (m_Width == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(widthPropertyKey, &m_Width, &value);
        m_Width = value;
        RIVE_EDITOR_CHANGED(widthChanged());
        notifyPropertyChanged(widthPropertyKey);
    }

    inline float height() const { return m_Height; }
    void height(float value)
    {
        if (m_Height == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(heightPropertyKey, &m_Height, &value);
        m_Height = value;
        RIVE_EDITOR_CHANGED(heightChanged());
        notifyPropertyChanged(heightPropertyKey);
    }

    inline Id xId() const { return m_XId; }
    void xId(Id value)
    {
        if (m_XId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(xIdPropertyKey, &m_XId, &value);
        m_XId = value;
        RIVE_EDITOR_CHANGED(xIdChanged());
        notifyPropertyChanged(xIdPropertyKey);
    }

    inline Id yId() const { return m_YId; }
    void yId(Id value)
    {
        if (m_YId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(yIdPropertyKey, &m_YId, &value);
        m_YId = value;
        RIVE_EDITOR_CHANGED(yIdChanged());
        notifyPropertyChanged(yIdPropertyKey);
    }

    inline uint32_t joystickFlags() const { return m_JoystickFlags; }
    void joystickFlags(uint32_t value)
    {
        if (m_JoystickFlags == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(joystickFlagsPropertyKey,
                             &m_JoystickFlags,
                             &value);
        m_JoystickFlags = value;
        RIVE_EDITOR_CHANGED(joystickFlagsChanged());
        notifyPropertyChanged(joystickFlagsPropertyKey);
    }

    inline Id handleSourceId() const { return m_HandleSourceId; }
    void handleSourceId(Id value)
    {
        if (m_HandleSourceId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(handleSourceIdPropertyKey,
                             &m_HandleSourceId,
                             &value);
        m_HandleSourceId = value;
        RIVE_EDITOR_CHANGED(handleSourceIdChanged());
        notifyPropertyChanged(handleSourceIdPropertyKey);
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
        RIVE_EDITOR_COPY(object);
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
                m_XId = CoreIdType::runtimeDeserialize(reader);
                return true;
            case yIdPropertyKey:
                m_YId = CoreIdType::runtimeDeserialize(reader);
                return true;
            case joystickFlagsPropertyKey:
                m_JoystickFlags = CoreUintType::deserialize(reader);
                return true;
            case handleSourceIdPropertyKey:
                m_HandleSourceId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
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
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/joystick_ext.inl"
#endif
};
} // namespace rive

#endif
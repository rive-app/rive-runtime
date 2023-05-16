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
    static const uint16_t xIdPropertyKey = 301;
    static const uint16_t yIdPropertyKey = 302;

private:
    float m_X = 0.0f;
    float m_Y = 0.0f;
    uint32_t m_XId = -1;
    uint32_t m_YId = -1;

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

    Core* clone() const override;
    void copy(const JoystickBase& object)
    {
        m_X = object.m_X;
        m_Y = object.m_Y;
        m_XId = object.m_XId;
        m_YId = object.m_YId;
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
            case xIdPropertyKey:
                m_XId = CoreUintType::deserialize(reader);
                return true;
            case yIdPropertyKey:
                m_YId = CoreUintType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void xChanged() {}
    virtual void yChanged() {}
    virtual void xIdChanged() {}
    virtual void yIdChanged() {}
};
} // namespace rive

#endif
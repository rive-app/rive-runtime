#ifndef _RIVE_LINEAR_GRADIENT_BASE_HPP_
#define _RIVE_LINEAR_GRADIENT_BASE_HPP_
#include "rive/container_component.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class LinearGradientBase : public ContainerComponent
{
protected:
    typedef ContainerComponent Super;

public:
    static const uint16_t typeKey = 22;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case LinearGradientBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t startXPropertyKey = 42;
    static const uint16_t startYPropertyKey = 33;
    static const uint16_t endXPropertyKey = 34;
    static const uint16_t endYPropertyKey = 35;
    static const uint16_t opacityPropertyKey = 46;

private:
    float m_StartX = 0.0f;
    float m_StartY = 0.0f;
    float m_EndX = 0.0f;
    float m_EndY = 0.0f;
    float m_Opacity = 1.0f;

public:
    inline float startX() const { return m_StartX; }
    void startX(float value)
    {
        if (m_StartX == value)
        {
            return;
        }
        m_StartX = value;
        startXChanged();
    }

    inline float startY() const { return m_StartY; }
    void startY(float value)
    {
        if (m_StartY == value)
        {
            return;
        }
        m_StartY = value;
        startYChanged();
    }

    inline float endX() const { return m_EndX; }
    void endX(float value)
    {
        if (m_EndX == value)
        {
            return;
        }
        m_EndX = value;
        endXChanged();
    }

    inline float endY() const { return m_EndY; }
    void endY(float value)
    {
        if (m_EndY == value)
        {
            return;
        }
        m_EndY = value;
        endYChanged();
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

    Core* clone() const override;
    void copy(const LinearGradientBase& object)
    {
        m_StartX = object.m_StartX;
        m_StartY = object.m_StartY;
        m_EndX = object.m_EndX;
        m_EndY = object.m_EndY;
        m_Opacity = object.m_Opacity;
        ContainerComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case startXPropertyKey:
                m_StartX = CoreDoubleType::deserialize(reader);
                return true;
            case startYPropertyKey:
                m_StartY = CoreDoubleType::deserialize(reader);
                return true;
            case endXPropertyKey:
                m_EndX = CoreDoubleType::deserialize(reader);
                return true;
            case endYPropertyKey:
                m_EndY = CoreDoubleType::deserialize(reader);
                return true;
            case opacityPropertyKey:
                m_Opacity = CoreDoubleType::deserialize(reader);
                return true;
        }
        return ContainerComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void startXChanged() {}
    virtual void startYChanged() {}
    virtual void endXChanged() {}
    virtual void endYChanged() {}
    virtual void opacityChanged() {}
};
} // namespace rive

#endif
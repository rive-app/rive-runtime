#ifndef _RIVE_TRANSFORM_COMPONENT_BASE_HPP_
#define _RIVE_TRANSFORM_COMPONENT_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/world_transform_component.hpp"
namespace rive
{
class TransformComponentBase : public WorldTransformComponent
{
protected:
    typedef WorldTransformComponent Super;

public:
    static const uint16_t typeKey = 38;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TransformComponentBase::typeKey:
            case WorldTransformComponentBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t rotationPropertyKey = 15;
    static const uint16_t scaleXPropertyKey = 16;
    static const uint16_t scaleYPropertyKey = 17;

private:
    float m_Rotation = 0.0f;
    float m_ScaleX = 1.0f;
    float m_ScaleY = 1.0f;

public:
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

    void copy(const TransformComponentBase& object)
    {
        m_Rotation = object.m_Rotation;
        m_ScaleX = object.m_ScaleX;
        m_ScaleY = object.m_ScaleY;
        WorldTransformComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
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
        return WorldTransformComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void rotationChanged() {}
    virtual void scaleXChanged() {}
    virtual void scaleYChanged() {}
};
} // namespace rive

#endif
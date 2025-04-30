#ifndef _RIVE_NODE_BASE_HPP_
#define _RIVE_NODE_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/transform_component.hpp"
namespace rive
{
class NodeBase : public TransformComponent
{
protected:
    typedef TransformComponent Super;

public:
    static const uint16_t typeKey = 2;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case NodeBase::typeKey:
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

    static const uint16_t xPropertyKey = 13;
    static const uint16_t xArtboardPropertyKey = 9;
    static const uint16_t yPropertyKey = 14;
    static const uint16_t yArtboardPropertyKey = 10;
    static const uint16_t computedLocalXPropertyKey = 806;
    static const uint16_t computedLocalYPropertyKey = 807;
    static const uint16_t computedWorldXPropertyKey = 808;
    static const uint16_t computedWorldYPropertyKey = 809;
    static const uint16_t computedWidthPropertyKey = 810;
    static const uint16_t computedHeightPropertyKey = 811;

protected:
    float m_X = 0.0f;
    float m_Y = 0.0f;

public:
    inline float x() const override { return m_X; }
    void x(float value)
    {
        if (m_X == value)
        {
            return;
        }
        m_X = value;
        xChanged();
    }

    inline float y() const override { return m_Y; }
    void y(float value)
    {
        if (m_Y == value)
        {
            return;
        }
        m_Y = value;
        yChanged();
    }

    virtual void setComputedLocalX(float value) = 0;
    virtual float computedLocalX() = 0;
    void computedLocalX(float value)
    {
        if (computedLocalX() == value)
        {
            return;
        }
        setComputedLocalX(value);
        computedLocalXChanged();
    }

    virtual void setComputedLocalY(float value) = 0;
    virtual float computedLocalY() = 0;
    void computedLocalY(float value)
    {
        if (computedLocalY() == value)
        {
            return;
        }
        setComputedLocalY(value);
        computedLocalYChanged();
    }

    virtual void setComputedWorldX(float value) = 0;
    virtual float computedWorldX() = 0;
    void computedWorldX(float value)
    {
        if (computedWorldX() == value)
        {
            return;
        }
        setComputedWorldX(value);
        computedWorldXChanged();
    }

    virtual void setComputedWorldY(float value) = 0;
    virtual float computedWorldY() = 0;
    void computedWorldY(float value)
    {
        if (computedWorldY() == value)
        {
            return;
        }
        setComputedWorldY(value);
        computedWorldYChanged();
    }

    virtual void setComputedWidth(float value) = 0;
    virtual float computedWidth() = 0;
    void computedWidth(float value)
    {
        if (computedWidth() == value)
        {
            return;
        }
        setComputedWidth(value);
        computedWidthChanged();
    }

    virtual void setComputedHeight(float value) = 0;
    virtual float computedHeight() = 0;
    void computedHeight(float value)
    {
        if (computedHeight() == value)
        {
            return;
        }
        setComputedHeight(value);
        computedHeightChanged();
    }

    Core* clone() const override;
    void copy(const NodeBase& object)
    {
        m_X = object.m_X;
        m_Y = object.m_Y;
        TransformComponent::copy(object);
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
        }
        return TransformComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void xChanged() {}
    virtual void yChanged() {}
    virtual void computedLocalXChanged() {}
    virtual void computedLocalYChanged() {}
    virtual void computedWorldXChanged() {}
    virtual void computedWorldYChanged() {}
    virtual void computedWidthChanged() {}
    virtual void computedHeightChanged() {}
};
} // namespace rive

#endif
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

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
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

private:
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
};
} // namespace rive

#endif
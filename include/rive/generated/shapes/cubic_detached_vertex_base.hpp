#ifndef _RIVE_CUBIC_DETACHED_VERTEX_BASE_HPP_
#define _RIVE_CUBIC_DETACHED_VERTEX_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/shapes/cubic_vertex.hpp"
namespace rive
{
class CubicDetachedVertexBase : public CubicVertex
{
protected:
    typedef CubicVertex Super;

public:
    static const uint16_t typeKey = 6;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case CubicDetachedVertexBase::typeKey:
            case CubicVertexBase::typeKey:
            case PathVertexBase::typeKey:
            case VertexBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t inRotationPropertyKey = 84;
    static const uint16_t inDistancePropertyKey = 85;
    static const uint16_t outRotationPropertyKey = 86;
    static const uint16_t outDistancePropertyKey = 87;

private:
    float m_InRotation = 0.0f;
    float m_InDistance = 0.0f;
    float m_OutRotation = 0.0f;
    float m_OutDistance = 0.0f;

public:
    inline float inRotation() const { return m_InRotation; }
    void inRotation(float value)
    {
        if (m_InRotation == value)
        {
            return;
        }
        m_InRotation = value;
        inRotationChanged();
    }

    inline float inDistance() const { return m_InDistance; }
    void inDistance(float value)
    {
        if (m_InDistance == value)
        {
            return;
        }
        m_InDistance = value;
        inDistanceChanged();
    }

    inline float outRotation() const { return m_OutRotation; }
    void outRotation(float value)
    {
        if (m_OutRotation == value)
        {
            return;
        }
        m_OutRotation = value;
        outRotationChanged();
    }

    inline float outDistance() const { return m_OutDistance; }
    void outDistance(float value)
    {
        if (m_OutDistance == value)
        {
            return;
        }
        m_OutDistance = value;
        outDistanceChanged();
    }

    Core* clone() const override;
    void copy(const CubicDetachedVertexBase& object)
    {
        m_InRotation = object.m_InRotation;
        m_InDistance = object.m_InDistance;
        m_OutRotation = object.m_OutRotation;
        m_OutDistance = object.m_OutDistance;
        CubicVertex::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case inRotationPropertyKey:
                m_InRotation = CoreDoubleType::deserialize(reader);
                return true;
            case inDistancePropertyKey:
                m_InDistance = CoreDoubleType::deserialize(reader);
                return true;
            case outRotationPropertyKey:
                m_OutRotation = CoreDoubleType::deserialize(reader);
                return true;
            case outDistancePropertyKey:
                m_OutDistance = CoreDoubleType::deserialize(reader);
                return true;
        }
        return CubicVertex::deserialize(propertyKey, reader);
    }

protected:
    virtual void inRotationChanged() {}
    virtual void inDistanceChanged() {}
    virtual void outRotationChanged() {}
    virtual void outDistanceChanged() {}
};
} // namespace rive

#endif
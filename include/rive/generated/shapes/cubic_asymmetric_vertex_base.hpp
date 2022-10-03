#ifndef _RIVE_CUBIC_ASYMMETRIC_VERTEX_BASE_HPP_
#define _RIVE_CUBIC_ASYMMETRIC_VERTEX_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/shapes/cubic_vertex.hpp"
namespace rive
{
class CubicAsymmetricVertexBase : public CubicVertex
{
protected:
    typedef CubicVertex Super;

public:
    static const uint16_t typeKey = 34;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case CubicAsymmetricVertexBase::typeKey:
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

    static const uint16_t rotationPropertyKey = 79;
    static const uint16_t inDistancePropertyKey = 80;
    static const uint16_t outDistancePropertyKey = 81;

private:
    float m_Rotation = 0.0f;
    float m_InDistance = 0.0f;
    float m_OutDistance = 0.0f;

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
    void copy(const CubicAsymmetricVertexBase& object)
    {
        m_Rotation = object.m_Rotation;
        m_InDistance = object.m_InDistance;
        m_OutDistance = object.m_OutDistance;
        CubicVertex::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case rotationPropertyKey:
                m_Rotation = CoreDoubleType::deserialize(reader);
                return true;
            case inDistancePropertyKey:
                m_InDistance = CoreDoubleType::deserialize(reader);
                return true;
            case outDistancePropertyKey:
                m_OutDistance = CoreDoubleType::deserialize(reader);
                return true;
        }
        return CubicVertex::deserialize(propertyKey, reader);
    }

protected:
    virtual void rotationChanged() {}
    virtual void inDistanceChanged() {}
    virtual void outDistanceChanged() {}
};
} // namespace rive

#endif
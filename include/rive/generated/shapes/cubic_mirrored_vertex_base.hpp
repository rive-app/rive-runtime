#ifndef _RIVE_CUBIC_MIRRORED_VERTEX_BASE_HPP_
#define _RIVE_CUBIC_MIRRORED_VERTEX_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/shapes/cubic_vertex.hpp"
namespace rive
{
class CubicMirroredVertexBase : public CubicVertex
{
protected:
    typedef CubicVertex Super;

public:
    static const uint16_t typeKey = 35;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case CubicMirroredVertexBase::typeKey:
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

    static const uint16_t rotationPropertyKey = 82;
    static const uint16_t distancePropertyKey = 83;

private:
    float m_Rotation = 0.0f;
    float m_Distance = 0.0f;

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

    inline float distance() const { return m_Distance; }
    void distance(float value)
    {
        if (m_Distance == value)
        {
            return;
        }
        m_Distance = value;
        distanceChanged();
    }

    Core* clone() const override;
    void copy(const CubicMirroredVertexBase& object)
    {
        m_Rotation = object.m_Rotation;
        m_Distance = object.m_Distance;
        CubicVertex::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case rotationPropertyKey:
                m_Rotation = CoreDoubleType::deserialize(reader);
                return true;
            case distancePropertyKey:
                m_Distance = CoreDoubleType::deserialize(reader);
                return true;
        }
        return CubicVertex::deserialize(propertyKey, reader);
    }

protected:
    virtual void rotationChanged() {}
    virtual void distanceChanged() {}
};
} // namespace rive

#endif
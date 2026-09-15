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

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
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

protected:
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
        RIVE_EDITOR_CHANGING(rotationPropertyKey, &m_Rotation, &value);
        m_Rotation = value;
        RIVE_EDITOR_CHANGED(rotationChanged());
        notifyPropertyChanged(rotationPropertyKey);
    }

    inline float inDistance() const { return m_InDistance; }
    void inDistance(float value)
    {
        if (m_InDistance == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(inDistancePropertyKey, &m_InDistance, &value);
        m_InDistance = value;
        RIVE_EDITOR_CHANGED(inDistanceChanged());
        notifyPropertyChanged(inDistancePropertyKey);
    }

    inline float outDistance() const { return m_OutDistance; }
    void outDistance(float value)
    {
        if (m_OutDistance == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(outDistancePropertyKey, &m_OutDistance, &value);
        m_OutDistance = value;
        RIVE_EDITOR_CHANGED(outDistanceChanged());
        notifyPropertyChanged(outDistancePropertyKey);
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
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/shapes/cubic_asymmetric_vertex_ext.inl"
#endif
};
} // namespace rive

#endif
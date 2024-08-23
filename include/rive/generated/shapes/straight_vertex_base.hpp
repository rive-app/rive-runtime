#ifndef _RIVE_STRAIGHT_VERTEX_BASE_HPP_
#define _RIVE_STRAIGHT_VERTEX_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/shapes/path_vertex.hpp"
namespace rive
{
class StraightVertexBase : public PathVertex
{
protected:
    typedef PathVertex Super;

public:
    static const uint16_t typeKey = 5;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case StraightVertexBase::typeKey:
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

    static const uint16_t radiusPropertyKey = 26;

private:
    float m_Radius = 0.0f;

public:
    inline float radius() const { return m_Radius; }
    void radius(float value)
    {
        if (m_Radius == value)
        {
            return;
        }
        m_Radius = value;
        radiusChanged();
    }

    Core* clone() const override;
    void copy(const StraightVertexBase& object)
    {
        m_Radius = object.m_Radius;
        PathVertex::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case radiusPropertyKey:
                m_Radius = CoreDoubleType::deserialize(reader);
                return true;
        }
        return PathVertex::deserialize(propertyKey, reader);
    }

protected:
    virtual void radiusChanged() {}
};
} // namespace rive

#endif
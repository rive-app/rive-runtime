#ifndef _RIVE_MESH_VERTEX_BASE_HPP_
#define _RIVE_MESH_VERTEX_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/shapes/vertex.hpp"
namespace rive
{
class MeshVertexBase : public Vertex
{
protected:
    typedef Vertex Super;

public:
    static const uint16_t typeKey = 108;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case MeshVertexBase::typeKey:
            case VertexBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t uPropertyKey = 215;
    static const uint16_t vPropertyKey = 216;

private:
    float m_U = 0.0f;
    float m_V = 0.0f;

public:
    inline float u() const { return m_U; }
    void u(float value)
    {
        if (m_U == value)
        {
            return;
        }
        m_U = value;
        uChanged();
    }

    inline float v() const { return m_V; }
    void v(float value)
    {
        if (m_V == value)
        {
            return;
        }
        m_V = value;
        vChanged();
    }

    Core* clone() const override;
    void copy(const MeshVertexBase& object)
    {
        m_U = object.m_U;
        m_V = object.m_V;
        Vertex::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case uPropertyKey:
                m_U = CoreDoubleType::deserialize(reader);
                return true;
            case vPropertyKey:
                m_V = CoreDoubleType::deserialize(reader);
                return true;
        }
        return Vertex::deserialize(propertyKey, reader);
    }

protected:
    virtual void uChanged() {}
    virtual void vChanged() {}
};
} // namespace rive

#endif
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

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t uPropertyKey = 215;
    static const uint16_t vPropertyKey = 216;

protected:
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
        RIVE_EDITOR_CHANGING(uPropertyKey, &m_U, &value);
        m_U = value;
        notifyPropertyChanged(uPropertyKey);
    }

    inline float v() const { return m_V; }
    void v(float value)
    {
        if (m_V == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(vPropertyKey, &m_V, &value);
        m_V = value;
        notifyPropertyChanged(vPropertyKey);
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
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/shapes/mesh_vertex_ext.inl"
#endif
};
} // namespace rive

#endif
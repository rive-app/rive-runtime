#ifndef _RIVE_PATH_VERTEX_BASE_HPP_
#define _RIVE_PATH_VERTEX_BASE_HPP_
#include "rive/shapes/vertex.hpp"
namespace rive
{
class PathVertexBase : public Vertex
{
protected:
    typedef Vertex Super;

public:
    static const uint16_t typeKey = 14;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
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

protected:
};
} // namespace rive

#endif
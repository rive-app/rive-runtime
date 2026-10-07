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

    uint16_t coreType() const override { return typeKey; }
};
} // namespace rive

#endif
#ifndef _RIVE_PATH_VERTEX_HPP_
#define _RIVE_PATH_VERTEX_HPP_
#include "rive/bones/weight.hpp"
#include "rive/generated/shapes/path_vertex_base.hpp"
#include "rive/math/mat2d.hpp"
namespace rive
{
class PathVertex : public PathVertexBase
{

public:
    StatusCode onAddedDirty(CoreContext* context) override;
    void markGeometryDirty() override;
};
} // namespace rive

#endif
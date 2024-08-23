#ifndef _RIVE_CUBIC_MIRRORED_VERTEX_HPP_
#define _RIVE_CUBIC_MIRRORED_VERTEX_HPP_
#include "rive/generated/shapes/cubic_mirrored_vertex_base.hpp"
namespace rive
{
class CubicMirroredVertex : public CubicMirroredVertexBase
{
protected:
    void computeIn() override;
    void computeOut() override;
    void rotationChanged() override;
    void distanceChanged() override;
};
} // namespace rive

#endif
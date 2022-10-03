#ifndef _RIVE_CUBIC_ASYMMETRIC_VERTEX_HPP_
#define _RIVE_CUBIC_ASYMMETRIC_VERTEX_HPP_
#include "rive/generated/shapes/cubic_asymmetric_vertex_base.hpp"
namespace rive
{
class CubicAsymmetricVertex : public CubicAsymmetricVertexBase
{
protected:
    void computeIn() override;
    void computeOut() override;
    void rotationChanged() override;
    void inDistanceChanged() override;
    void outDistanceChanged() override;
};
} // namespace rive

#endif
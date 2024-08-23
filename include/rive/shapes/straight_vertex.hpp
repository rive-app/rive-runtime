#ifndef _RIVE_STRAIGHT_VERTEX_HPP_
#define _RIVE_STRAIGHT_VERTEX_HPP_
#include "rive/generated/shapes/straight_vertex_base.hpp"
namespace rive
{
class StraightVertex : public StraightVertexBase
{
protected:
    void radiusChanged() override;
};
} // namespace rive

#endif
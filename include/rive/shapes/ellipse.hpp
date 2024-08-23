#ifndef _RIVE_ELLIPSE_HPP_
#define _RIVE_ELLIPSE_HPP_
#include "rive/generated/shapes/ellipse_base.hpp"
#include "rive/shapes/cubic_detached_vertex.hpp"

namespace rive
{
class Ellipse : public EllipseBase
{
    CubicDetachedVertex m_Vertex1, m_Vertex2, m_Vertex3, m_Vertex4;

public:
    Ellipse();
    void update(ComponentDirt value) override;
};
} // namespace rive

#endif
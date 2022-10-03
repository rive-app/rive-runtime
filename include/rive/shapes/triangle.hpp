#ifndef _RIVE_TRIANGLE_HPP_
#define _RIVE_TRIANGLE_HPP_
#include "rive/generated/shapes/triangle_base.hpp"
#include "rive/shapes/straight_vertex.hpp"

namespace rive
{
class Triangle : public TriangleBase
{
private:
    StraightVertex m_Vertex1, m_Vertex2, m_Vertex3;

public:
    Triangle();
    void update(ComponentDirt value) override;
};
} // namespace rive

#endif

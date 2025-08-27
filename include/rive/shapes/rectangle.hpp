#ifndef _RIVE_RECTANGLE_HPP_
#define _RIVE_RECTANGLE_HPP_
#include "rive/generated/shapes/rectangle_base.hpp"
#include "rive/shapes/straight_vertex.hpp"

namespace rive
{
class Rectangle : public RectangleBase
{
    StraightVertex m_Vertex1, m_Vertex2, m_Vertex3, m_Vertex4;

public:
    Rectangle();
    void update(ComponentDirt value) override;

protected:
    void cornerRadiusTLChanged() override;
    void cornerRadiusTRChanged() override;
    void cornerRadiusBLChanged() override;
    void cornerRadiusBRChanged() override;
};
} // namespace rive

#endif
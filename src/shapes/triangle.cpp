#include "rive/shapes/triangle.hpp"
#include "rive/component_dirt.hpp"
#include "rive/math/circle_constant.hpp"

using namespace rive;

Triangle::Triangle()
{
    addVertex(&m_Vertex1);
    addVertex(&m_Vertex2);
    addVertex(&m_Vertex3);
}

void Triangle::update(ComponentDirt value)
{
    if (hasDirt(value, ComponentDirt::Path))
    {
        auto ox = -originX() * width();
        auto oy = -originY() * height();

        m_Vertex1.x(ox + width() / 2);
        m_Vertex1.y(oy);

        m_Vertex2.x(ox + width());
        m_Vertex2.y(oy + height());

        m_Vertex3.x(ox);
        m_Vertex3.y(oy + height());
    }

    Super::update(value);
}
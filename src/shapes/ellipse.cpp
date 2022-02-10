#include "rive/shapes/ellipse.hpp"
#include "rive/component_dirt.hpp"
#include "rive/math/circle_constant.hpp"

using namespace rive;

Ellipse::Ellipse()
{
    addVertex(&m_Vertex1);
    addVertex(&m_Vertex2);
    addVertex(&m_Vertex3);
    addVertex(&m_Vertex4);
}

void Ellipse::update(ComponentDirt value)
{
    if (hasDirt(value, ComponentDirt::Path))
    {
        auto radiusX = width() / 2.0f;
        auto radiusY = height() / 2.0f;

        auto ox = -originX() * width() + radiusX;
        auto oy = -originY() * height() + radiusY;

        m_Vertex1.x(ox);
        m_Vertex1.y(oy - radiusY);
        m_Vertex1.inPoint(Vec2D(ox - radiusX * circleConstant, oy - radiusY));
        m_Vertex1.outPoint(Vec2D(ox + radiusX * circleConstant, oy - radiusY));

        m_Vertex2.x(ox + radiusX);
        m_Vertex2.y(oy);
        m_Vertex2.inPoint(Vec2D(ox + radiusX, oy + circleConstant * -radiusY));
        m_Vertex2.outPoint(Vec2D(ox + radiusX, oy + circleConstant * radiusY));

        m_Vertex3.x(ox);
        m_Vertex3.y(oy + radiusY);
        m_Vertex3.inPoint(Vec2D(ox + radiusX * circleConstant, oy + radiusY));
        m_Vertex3.outPoint(Vec2D(ox - radiusX * circleConstant, oy + radiusY));

        m_Vertex4.x(ox - radiusX);
        m_Vertex4.y(oy);
        m_Vertex4.inPoint(Vec2D(ox - radiusX, oy + radiusY * circleConstant));
        m_Vertex4.outPoint(Vec2D(ox - radiusX, oy - radiusY * circleConstant));
    }

    Super::update(value);
}
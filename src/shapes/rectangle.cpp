#include "rive/shapes/rectangle.hpp"

using namespace rive;

Rectangle::Rectangle()
{
    addVertex(&m_Vertex1);
    addVertex(&m_Vertex2);
    addVertex(&m_Vertex3);
    addVertex(&m_Vertex4);
}

void Rectangle::cornerRadiusTLChanged() { markPathDirty(); }
void Rectangle::cornerRadiusTRChanged() { markPathDirty(); }
void Rectangle::cornerRadiusBLChanged() { markPathDirty(); }
void Rectangle::cornerRadiusBRChanged() { markPathDirty(); }

void Rectangle::update(ComponentDirt value)
{
    if (hasDirt(value, ComponentDirt::Path))
    {
        auto radius = cornerRadiusTL();
        auto link = linkCornerRadius();

        auto ox = -originX() * width();
        auto oy = -originY() * height();

        m_Vertex1.x(ox);
        m_Vertex1.y(oy);
        m_Vertex1.radius(radius);

        m_Vertex2.x(ox + width());
        m_Vertex2.y(oy);
        m_Vertex2.radius(link ? radius : cornerRadiusTR());

        m_Vertex3.x(ox + width());
        m_Vertex3.y(oy + height());
        m_Vertex3.radius(link ? radius : cornerRadiusBR());

        m_Vertex4.x(ox);
        m_Vertex4.y(oy + height());
        m_Vertex4.radius(link ? radius : cornerRadiusBL());
    }

    Super::update(value);
}
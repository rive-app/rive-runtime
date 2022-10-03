#include "rive/shapes/star.hpp"
#include "rive/shapes/straight_vertex.hpp"
#include "rive/math/math_types.hpp"
#include <cmath>
#include <cstdio>

using namespace rive;

Star::Star() {}

void Star::innerRadiusChanged() { markPathDirty(); }

std::size_t Star::vertexCount() { return points() * 2; }

void Star::buildPolygon()
{
    auto halfWidth = width() / 2;
    auto halfHeight = height() / 2;
    auto innerHalfWidth = width() * innerRadius() / 2;
    auto innerHalfHeight = height() * innerRadius() / 2;
    auto ox = -originX() * width() + halfWidth;
    auto oy = -originY() * height() + halfHeight;

    std::size_t length = vertexCount();
    auto angle = -math::PI / 2;
    auto inc = 2 * math::PI / length;

    for (std::size_t i = 0; i < length; i += 2)
    {
        {
            StraightVertex& vertex = m_PolygonVertices[i];
            vertex.x(ox + cos(angle) * halfWidth);
            vertex.y(oy + sin(angle) * halfHeight);
            vertex.radius(cornerRadius());
            angle += inc;
        }
        {
            StraightVertex& vertex = m_PolygonVertices[i + 1];
            vertex.x(ox + cos(angle) * innerHalfWidth);
            vertex.y(oy + sin(angle) * innerHalfHeight);
            vertex.radius(cornerRadius());
            angle += inc;
        }
    }
}

void Star::update(ComponentDirt value) { Super::update(value); }

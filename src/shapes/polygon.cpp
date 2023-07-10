#include "rive/shapes/polygon.hpp"
#include "rive/shapes/star.hpp"
#include "rive/shapes/straight_vertex.hpp"
#include "rive/math/math_types.hpp"
#include <cmath>

using namespace rive;

Polygon::Polygon() {}

Polygon::~Polygon() {}

void Polygon::cornerRadiusChanged() { markPathDirty(); }

void Polygon::pointsChanged() { markPathDirty(); }

std::size_t Polygon::vertexCount() { return points(); }

void Polygon::buildPolygon()
{
    auto halfWidth = width() / 2;
    auto halfHeight = height() / 2;

    auto ox = -originX() * width() + halfWidth;
    auto oy = -originY() * height() + halfHeight;

    auto angle = -math::PI / 2;
    auto inc = 2 * math::PI / points();

    for (StraightVertex& vertex : m_PolygonVertices)
    {
        vertex.x(ox + cos(angle) * halfWidth);
        vertex.y(oy + sin(angle) * halfHeight);
        vertex.radius(cornerRadius());
        angle += inc;
    }
}

void Polygon::update(ComponentDirt value)
{
    if (hasDirt(value, ComponentDirt::Path))
    {
        if (m_PolygonVertices.size() != vertexCount())
        {
            m_PolygonVertices.resize(vertexCount());
            m_Vertices.clear();
            for (StraightVertex& vertex : m_PolygonVertices)
            {
                m_Vertices.push_back(&vertex);
            }
        }
        buildPolygon();
    }
    Super::update(value);
}

#include "math/pi.hpp"
#include "shapes/polygon.hpp"
#include "shapes/star.hpp"
#include "shapes/straight_vertex.hpp"
#include <cmath>

using namespace rive;

void Polygon::cornerRadiusChanged() { markPathDirty(); }

void Polygon::pointsChanged() { markPathDirty(); }

void Polygon::clearVertices()
{
	for (auto vertex: m_Vertices)
	{
		delete vertex;
	}
	m_Vertices.clear();
}

void Polygon::update(ComponentDirt value)
{
	if (hasDirt(value, ComponentDirt::Path))
	{
		clearVertices();

		auto halfWidth = width() / 2;
		auto halfHeight = height() / 2;
		auto angle = -pi / 2;
		auto inc = 2 * pi / points();

		for (int i = 0; i < points(); i++)
		{
			StraightVertex* vertex = new StraightVertex();
			vertex->x(cos(angle) * halfWidth);
			vertex->y(sin(angle) * halfHeight);
			vertex->radius(cornerRadius());
			addVertex(vertex);
			angle += inc;
		}
	}

	Super::update(value);
}
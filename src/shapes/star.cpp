#include "shapes/star.hpp"
#include "shapes/straight_vertex.hpp"
#include <cmath>

using namespace rive;

Star::Star() {}

void Star::innerRadiusChanged() { markPathDirty(); }

int Star::expectedSize() { return points() * 2; }

void Star::buildPolygon()
{
	auto actualPoints = expectedSize();
	auto halfWidth = width() / 2;
	auto halfHeight = height() / 2;
	auto innerHalfWidth = width() * innerRadius() / 2;
	auto innerHalfHeight = height() * innerRadius() / 2;

	auto angle = -M_PI / 2;
	auto inc = 2 * M_PI / actualPoints;

	for (int i = 0; i < actualPoints; i++)
	{
		auto isInner = i & 1;
		if (isInner)
		{
			buildVertex(m_Vertices[i], innerHalfHeight, innerHalfWidth, angle);
		}
		else
		{
			buildVertex(m_Vertices[i], halfHeight, halfWidth, angle);
		}
		angle += inc;
	}
}

void Star::update(ComponentDirt value) { Super::update(value); }
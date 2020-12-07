#include "math/pi.hpp"
#include "shapes/star.hpp"
#include "shapes/straight_vertex.hpp"
#include <cmath>

using namespace rive;

void Star::innerRadiusChanged() { markPathDirty(); }

void Star::update(ComponentDirt value)
{
	if (hasDirt(value, ComponentDirt::Path))
	{
		clearVertices();

		auto actualPoints = points() * 2;
		auto halfWidth = width() / 2;
		auto halfHeight = height() / 2;
		auto innerHalfWidth = width() * innerRadius() / 2;
		auto innerHalfHeight = height() * innerRadius() / 2;

		auto angle = -pi / 2;
		auto inc = 2 * pi / actualPoints;

		for (int i = 0; i < points(); i++)
		{
			StraightVertex* outerVertex = new StraightVertex();
			outerVertex->x(cos(angle) * halfWidth);
			outerVertex->y(sin(angle) * halfHeight);
			outerVertex->radius(cornerRadius());
			addVertex(outerVertex);
			angle += inc;

			StraightVertex* innerVertex = new StraightVertex();
			innerVertex->x(cos(angle) * innerHalfWidth);
			innerVertex->y(sin(angle) * innerHalfHeight);
			innerVertex->radius(cornerRadius());
			addVertex(innerVertex);
			angle += inc;
		}
	}

	ParametricPath::update(value);
}
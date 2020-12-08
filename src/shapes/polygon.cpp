#include "shapes/polygon.hpp"
#include "shapes/star.hpp"
#include "shapes/straight_vertex.hpp"
#include <cmath>

using namespace rive;

Polygon::Polygon() {}

Polygon::~Polygon()
{
	for (auto vertex: m_Vertices)
	{
		delete vertex;
	}
}

void Polygon::cornerRadiusChanged() { markPathDirty(); }

void Polygon::pointsChanged() { markPathDirty(); }

void Polygon::resizeVertices(int newSize)
{
	auto currentSize = m_Vertices.size();

	if (newSize == currentSize)
	{
		return;
	}

	if (newSize > currentSize)
	{
		m_Vertices.resize(newSize);
		for (int i = currentSize; i < newSize; i++)
		{
			m_Vertices[i] = new StraightVertex();
		}
	}
	else
	{
		for (int i = newSize; i < currentSize; i++)
		{
			delete m_Vertices[i];
		}
		m_Vertices.resize(newSize);
	}
}

int Polygon::expectedSize()
{
	return points();
}

void Polygon::buildVertex(PathVertex* vertex, float h, float w, float angle)
{
	vertex->x(cos(angle) * w);
	vertex->y(sin(angle) * h);
	vertex->as<StraightVertex>()->radius(cornerRadius());
}

void Polygon::buildPolygon()
{
	auto halfWidth = width() / 2;
	auto halfHeight = height() / 2;
	auto angle = -M_PI / 2;
	auto inc = 2 * M_PI / points();

	for (int i = 0; i < points(); i++)
	{
		buildVertex(m_Vertices[i], halfHeight, halfWidth, angle);
		angle += inc;
	}
}

void Polygon::update(ComponentDirt value)
{
	if (hasDirt(value, ComponentDirt::Path))
	{
		if (m_Vertices.size() != expectedSize())
		{
			resizeVertices(expectedSize());
		}
		buildPolygon();
	}
	Super::update(value);
}
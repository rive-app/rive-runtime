#include "shapes/rectangle.hpp"

using namespace rive;

Rectangle::Rectangle()
{
	addVertex(&m_Vertex1);
	addVertex(&m_Vertex2);
	addVertex(&m_Vertex3);
	addVertex(&m_Vertex4);
}

void Rectangle::cornerRadiusChanged() { markPathDirty(); }

void Rectangle::update(ComponentDirt value)
{
	if (hasDirt(value, ComponentDirt::Path))
	{
		auto halfWidth = width() / 2.0f;
		auto halfHeight = height() / 2.0f;
		auto radius = cornerRadius();

		m_Vertex1.x(-halfWidth);
		m_Vertex1.y(-halfHeight);
		m_Vertex1.radius(radius);

		m_Vertex2.x(halfWidth);
		m_Vertex2.y(-halfHeight);
		m_Vertex2.radius(radius);

		m_Vertex3.x(halfWidth);
		m_Vertex3.y(halfHeight);
		m_Vertex3.radius(radius);

		m_Vertex4.x(-halfWidth);
		m_Vertex4.y(halfHeight);
		m_Vertex4.radius(radius);
	}
    
	Super::update(value);
}
#include "shapes/ellipse.hpp"
#include "component_dirt.hpp"
#include "math/circle_constant.hpp"

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

		m_Vertex1.x(0.0f);
		m_Vertex1.y(-radiusY);
		m_Vertex1.inPoint(Vec2D(-radiusX * circleConstant, -radiusY));
		m_Vertex1.outPoint(Vec2D(radiusX * circleConstant, -radiusY));

		m_Vertex2.x(radiusX);
		m_Vertex2.y(0.0f);
		m_Vertex2.inPoint(Vec2D(radiusX, circleConstant * -radiusY));
		m_Vertex2.outPoint(Vec2D(radiusX, circleConstant * radiusY));

		m_Vertex3.x(0.0f);
		m_Vertex3.y(radiusY);
		m_Vertex3.inPoint(Vec2D(radiusX * circleConstant, radiusY));
		m_Vertex3.outPoint(Vec2D(-radiusX * circleConstant, radiusY));

		m_Vertex4.x(-radiusX);
		m_Vertex4.y(0.0f);
		m_Vertex4.inPoint(Vec2D(-radiusX, radiusY * circleConstant));
		m_Vertex4.outPoint(Vec2D(-radiusX, -radiusY * circleConstant));
	}

	Super::update(value);
}
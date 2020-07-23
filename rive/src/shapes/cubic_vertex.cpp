#include "shapes/cubic_vertex.hpp"

using namespace rive;

const Vec2D& CubicVertex::inPoint()
{
	if (!m_InValid)
	{
		computeIn();
		m_InValid = true;
	}
	return m_InPoint;
}

const Vec2D& CubicVertex::outPoint()
{
	if (!m_OutValid)
	{
		computeOut();
		m_OutValid = true;
	}
	return m_OutPoint;
}

void CubicVertex::outPoint(const Vec2D& value)
{
	Vec2D::copy(m_OutPoint, value);
	m_OutValid = true;
}

void CubicVertex::inPoint(const Vec2D& value)
{
	Vec2D::copy(m_InPoint, value);
	m_InValid = true;
}
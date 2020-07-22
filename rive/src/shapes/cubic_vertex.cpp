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
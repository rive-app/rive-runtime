#include "rive/shapes/cubic_mirrored_vertex.hpp"
#include "rive/math/vec2d.hpp"
#include <cmath>

using namespace rive;

void CubicMirroredVertex::computeIn()
{
	Vec2D::add(
	    m_InPoint,
	    Vec2D(x(), y()),
	    Vec2D(cos(rotation()) * -distance(), sin(rotation()) * -distance()));
}

void CubicMirroredVertex::computeOut()
{
	Vec2D::add(
	    m_OutPoint,
	    Vec2D(x(), y()),
	    Vec2D(cos(rotation()) * distance(), sin(rotation()) * distance()));
}

void CubicMirroredVertex::rotationChanged()
{
	m_InValid = m_OutValid = false;
	markPathDirty();
}
void CubicMirroredVertex::distanceChanged()
{
	m_InValid = m_OutValid = false;
	markPathDirty();
}
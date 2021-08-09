#include "rive/shapes/cubic_detached_vertex.hpp"
#include "rive/math/vec2d.hpp"
#include <cmath>

using namespace rive;

void CubicDetachedVertex::computeIn()
{
	Vec2D::add(m_InPoint,
	           Vec2D(x(), y()),
	           Vec2D(cos(inRotation()) * inDistance(),
	                 sin(inRotation()) * inDistance()));
}

void CubicDetachedVertex::computeOut()
{
	Vec2D::add(m_OutPoint,
	           Vec2D(x(), y()),
	           Vec2D(cos(outRotation()) * outDistance(),
	                 sin(outRotation()) * outDistance()));
}

void CubicDetachedVertex::inRotationChanged()
{
	m_InValid = false;
	markPathDirty();
}
void CubicDetachedVertex::inDistanceChanged()
{
	m_InValid = false;
	markPathDirty();
}
void CubicDetachedVertex::outRotationChanged()
{
	m_OutValid = false;
	markPathDirty();
}
void CubicDetachedVertex::outDistanceChanged()
{
	m_OutValid = false;
	markPathDirty();
}
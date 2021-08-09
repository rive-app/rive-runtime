#include "rive/shapes/cubic_asymmetric_vertex.hpp"
#include "rive/math/vec2d.hpp"
#include <cmath>

using namespace rive;

void CubicAsymmetricVertex::computeIn()
{
	Vec2D::add(m_InPoint,
	           Vec2D(x(), y()),
	           Vec2D(cos(rotation()) * -inDistance(),
	                 sin(rotation()) * -inDistance()));
}

void CubicAsymmetricVertex::computeOut()
{
	Vec2D::add(m_OutPoint,
	           Vec2D(x(), y()),
	           Vec2D(cos(rotation()) * outDistance(),
	                 sin(rotation()) * outDistance()));
}

void CubicAsymmetricVertex::rotationChanged()
{
	m_InValid = false;
	m_OutValid = false;
	markPathDirty();
}
void CubicAsymmetricVertex::inDistanceChanged()
{
	m_InValid = false;
	markPathDirty();
}
void CubicAsymmetricVertex::outDistanceChanged()
{
	m_OutValid = false;
	markPathDirty();
}
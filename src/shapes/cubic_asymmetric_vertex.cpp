#include "rive/shapes/cubic_asymmetric_vertex.hpp"
#include "rive/math/vec2d.hpp"
#include <cmath>

using namespace rive;

static Vec2D get_point(const CubicAsymmetricVertex& v) { return Vec2D(v.x(), v.y()); }

static Vec2D in_vector(const CubicAsymmetricVertex& v)
{
    return Vec2D(cos(v.rotation()) * v.inDistance(), sin(v.rotation()) * v.inDistance());
}

static Vec2D out_vector(const CubicAsymmetricVertex& v)
{
    return Vec2D(cos(v.rotation()) * v.outDistance(), sin(v.rotation()) * v.outDistance());
}

void CubicAsymmetricVertex::computeIn() { m_InPoint = get_point(*this) - in_vector(*this); }

void CubicAsymmetricVertex::computeOut() { m_OutPoint = get_point(*this) + out_vector(*this); }

void CubicAsymmetricVertex::rotationChanged()
{
    m_InValid = false;
    m_OutValid = false;
    markGeometryDirty();
}
void CubicAsymmetricVertex::inDistanceChanged()
{
    m_InValid = false;
    markGeometryDirty();
}
void CubicAsymmetricVertex::outDistanceChanged()
{
    m_OutValid = false;
    markGeometryDirty();
}

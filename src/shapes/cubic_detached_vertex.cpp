#include "rive/shapes/cubic_detached_vertex.hpp"
#include "rive/math/vec2d.hpp"
#include <cmath>

using namespace rive;

static Vec2D get_point(const CubicDetachedVertex& v) { return Vec2D(v.x(), v.y()); }

static Vec2D in_vector(const CubicDetachedVertex& v)
{
    return Vec2D(cos(v.inRotation()) * v.inDistance(), sin(v.inRotation()) * v.inDistance());
}

static Vec2D out_vector(const CubicDetachedVertex& v)
{
    return Vec2D(cos(v.outRotation()) * v.outDistance(), sin(v.outRotation()) * v.outDistance());
}

void CubicDetachedVertex::computeIn() { m_InPoint = get_point(*this) + in_vector(*this); }

void CubicDetachedVertex::computeOut() { m_OutPoint = get_point(*this) + out_vector(*this); }

void CubicDetachedVertex::inRotationChanged()
{
    m_InValid = false;
    markGeometryDirty();
}
void CubicDetachedVertex::inDistanceChanged()
{
    m_InValid = false;
    markGeometryDirty();
}
void CubicDetachedVertex::outRotationChanged()
{
    m_OutValid = false;
    markGeometryDirty();
}
void CubicDetachedVertex::outDistanceChanged()
{
    m_OutValid = false;
    markGeometryDirty();
}

#include "rive/shapes/cubic_mirrored_vertex.hpp"
#include "rive/math/vec2d.hpp"
#include <cmath>

using namespace rive;

static Vec2D get_point(const CubicMirroredVertex& v) { return Vec2D(v.x(), v.y()); }

static Vec2D get_vector(const CubicMirroredVertex& v)
{
    return Vec2D(cos(v.rotation()) * v.distance(), sin(v.rotation()) * v.distance());
}

void CubicMirroredVertex::computeIn() { m_InPoint = get_point(*this) - get_vector(*this); }

void CubicMirroredVertex::computeOut() { m_OutPoint = get_point(*this) + get_vector(*this); }

void CubicMirroredVertex::rotationChanged()
{
    m_InValid = m_OutValid = false;
    markGeometryDirty();
}
void CubicMirroredVertex::distanceChanged()
{
    m_InValid = m_OutValid = false;
    markGeometryDirty();
}

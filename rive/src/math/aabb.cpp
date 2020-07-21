#include "math/aabb.hpp"
#include <cmath>

using namespace rive;

AABB::AABB() : m_Buffer{0} {}

AABB::AABB(const AABB& copy)
{
	m_Buffer[0] = copy.m_Buffer[0];
	m_Buffer[1] = copy.m_Buffer[1];
	m_Buffer[2] = copy.m_Buffer[2];
	m_Buffer[3] = copy.m_Buffer[3];
}

AABB::AABB(float minX, float minY, float maxX, float maxY) : m_Buffer{minX, minY, maxX, maxY} {}

void AABB::center(Vec2D& out, const AABB& a)
{
	out[0] = (a[0] + a[2]) * 0.5f;
	out[1] = (a[1] + a[3]) * 0.5f;
}

void AABB::size(Vec2D& out, const AABB& a)
{
	out[0] = a[2] - a[0];
	out[1] = a[3] - a[1];
}

void AABB::extents(Vec2D& out, const AABB& a)
{
	out[0] = (a[2] - a[0]) * 0.5;
	out[1] = (a[3] - a[1]) * 0.5;
}

void AABB::combine(AABB& out, const AABB& a, const AABB& b) {}

bool AABB::contains(const AABB& a, const AABB& b) { return a[0] <= b[0] && a[1] <= b[1] && b[2] <= a[2] && b[3] <= a[3]; }

bool AABB::isValid(const AABB& a)
{
	float dx = a[2] - a[0];
	float dy = a[3] - a[1];
	return dx >= 0.0f && dy >= 0.0f && std::isfinite(a[0]) && std::isfinite(a[1]) && std::isfinite(a[2]) && std::isfinite(a[3]);
}

bool AABB::testOverlap(const AABB& a, const AABB& b)
{
	float d1x = b[0] - a[2];
	float d1y = b[1] - a[3];

	float d2x = a[0] - b[2];
	float d2y = a[1] - b[3];

	if (d1x > 0.0 || d1y > 0.0)
	{
		return false;
	}

	if (d2x > 0.0 || d2y > 0.0)
	{
		return false;
	}

	return true;
}

bool AABB::areIdentical(const AABB& a, const AABB& b) { return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3]; }

float AABB::width() const { return m_Max[0] - m_Min[0]; }

float AABB::height() const { return m_Max[1] - m_Min[1]; }

float AABB::perimeter() const
{
	float wx = m_Buffer[2] - m_Buffer[0];
	float wy = m_Buffer[3] - m_Buffer[1];
	return 2.0 * (wx + wy);
}

void AABB::transform(AABB& out, const AABB& a, const Mat2D& matrix)
{
	Vec2D p1(a[0], a[1]);
	Vec2D p2(a[2], a[1]);
	Vec2D p3(a[2], a[3]);
	Vec2D p4(a[0], a[3]);

	Vec2D::transform(p1, p1, matrix);
	Vec2D::transform(p2, p2, matrix);
	Vec2D::transform(p3, p3, matrix);
	Vec2D::transform(p4, p4, matrix);

	out[0] = std::fmin(p1[0], std::fmin(p2[0], std::fmin(p3[0], p4[0])));
	out[1] = std::fmin(p1[1], std::fmin(p2[1], std::fmin(p3[1], p4[1])));
	out[2] = std::fmax(p1[0], std::fmax(p2[0], std::fmax(p3[0], p4[0])));
	out[3] = std::fmax(p1[1], std::fmax(p2[1], std::fmax(p3[1], p4[1])));
}
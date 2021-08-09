#include "rive/math/vec2d.hpp"
#include "rive/math/mat2d.hpp"
#include <cmath>

using namespace rive;

Vec2D::Vec2D() : m_Buffer{0.0f, 0.0f} {}

Vec2D::Vec2D(float x, float y) : m_Buffer{x, y} {}

Vec2D::Vec2D(const Vec2D& copy) : m_Buffer{copy.m_Buffer[0], copy.m_Buffer[1]}
{
}

void Vec2D::transform(Vec2D& result, const Vec2D& a, const Mat2D& m)
{
	float x = a[0];
	float y = a[1];
	result[0] = m[0] * x + m[2] * y + m[4];
	result[1] = m[1] * x + m[3] * y + m[5];
}

void Vec2D::transformDir(Vec2D& result, const Vec2D& a, const Mat2D& m)
{
	float x = a[0];
	float y = a[1];
	result[0] = m[0] * x + m[2] * y;
	result[1] = m[1] * x + m[3] * y;
}

void Vec2D::add(Vec2D& result, const Vec2D& a, const Vec2D& b)
{
	result[0] = a[0] + b[0];
	result[1] = a[1] + b[1];
}

void Vec2D::subtract(Vec2D& result, const Vec2D& a, const Vec2D& b)
{
	result[0] = a[0] - b[0];
	result[1] = a[1] - b[1];
}

float Vec2D::length(const Vec2D& a) { return std::sqrt(lengthSquared(a)); }

float Vec2D::lengthSquared(const Vec2D& a)
{
	float x = a[0];
	float y = a[1];
	return x * x + y * y;
}

float Vec2D::distance(const Vec2D& a, const Vec2D& b)
{
	return std::sqrt(distanceSquared(a, b));
}

float Vec2D::distanceSquared(const Vec2D& a, const Vec2D& b)
{
	float x = b[0] - a[0];
	float y = b[1] - a[1];
	return x * x + y * y;
}

void Vec2D::copy(Vec2D& result, const Vec2D& a)
{
	result[0] = a[0];
	result[1] = a[1];
}

void Vec2D::normalize(Vec2D& result, const Vec2D& a)
{
	float x = a[0];
	float y = a[1];
	float len = x * x + y * y;
	if (len > 0.0f)
	{
		len = 1.0f / std::sqrt(len);
		result[0] = a[0] * len;
		result[1] = a[1] * len;
	}
}

float Vec2D::dot(const Vec2D& a, const Vec2D& b)
{
	return a[0] * b[0] + a[1] * b[1];
}

void Vec2D::lerp(Vec2D& o, const Vec2D& a, const Vec2D& b, float f)
{
	float ax = a[0];
	float ay = a[1];
	o[0] = ax + f * (b[0] - ax);
	o[1] = ay + f * (b[1] - ay);
}

void Vec2D::scale(Vec2D& o, const Vec2D& a, float scale)
{
	o[0] = a[0] * scale;
	o[1] = a[1] * scale;
}

void Vec2D::scaleAndAdd(Vec2D& o, const Vec2D& a, const Vec2D& b, float scale)
{
	o[0] = a[0] + b[0] * scale;
	o[1] = a[1] + b[1] * scale;
}

void Vec2D::negate(Vec2D& o, const Vec2D& a)
{
	o[0] = -a[0];
	o[1] = -a[1];
}
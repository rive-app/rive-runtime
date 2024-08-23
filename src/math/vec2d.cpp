#include "rive/math/vec2d.hpp"
#include "rive/math/mat2d.hpp"
#include <cmath>

using namespace rive;

Vec2D Vec2D::transformDir(const Vec2D& a, const Mat2D& m)
{
    return {
        m[0] * a.x + m[2] * a.y,
        m[1] * a.x + m[3] * a.y,
    };
}
Vec2D Vec2D::transformMat2D(const Vec2D& a, const Mat2D& m)
{
    return {m[0] * a.x + m[2] * a.y + m[4], m[1] * a.x + m[3] * a.y + m[5]};
}
float Vec2D::length() const { return std::sqrt(lengthSquared()); }

Vec2D Vec2D::normalized() const
{
    auto len2 = lengthSquared();
    auto scale = len2 > 0 ? (1 / std::sqrt(len2)) : 1;
    return *this * scale;
}

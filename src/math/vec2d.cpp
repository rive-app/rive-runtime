#include "rive/math/vec2d.hpp"
#include "rive/math/mat2d.hpp"
#include <cmath>

using namespace rive;

Vec2D Vec2D::transform(const Vec2D& a, const Mat2D& m) {
    return {
        m[0] * a.x() + m[2] * a.y() + m[4],
        m[1] * a.x() + m[3] * a.y() + m[5],
    };
}

Vec2D Vec2D::transformDir(const Vec2D& a, const Mat2D& m) {
    return {
        m[0] * a.x() + m[2] * a.y(),
        m[1] * a.x() + m[3] * a.y(),
    };
}

float Vec2D::distance(const Vec2D& a, const Vec2D& b) {
    return std::sqrt(distanceSquared(a, b));
}

float Vec2D::distanceSquared(const Vec2D& a, const Vec2D& b) {
    float x = b[0] - a[0];
    float y = b[1] - a[1];
    return x * x + y * y;
}

void Vec2D::copy(Vec2D& result, const Vec2D& a) {
    result[0] = a[0];
    result[1] = a[1];
}

float Vec2D::length() const {
    return std::sqrt(lengthSquared());
}

Vec2D Vec2D::normalized() const {
    auto len2 = lengthSquared();
    auto scale = len2 > 0 ? (1 / std::sqrt(len2)) : 1;
    return *this * scale;
}

float Vec2D::dot(const Vec2D& a, const Vec2D& b) {
    return a[0] * b[0] + a[1] * b[1];
}

Vec2D Vec2D::lerp(const Vec2D& a, const Vec2D& b, float f) {
    return {
        a.x() + f * (b[0] - a.x()),
        a.y() + f * (b[1] - a.y()),
    };
}

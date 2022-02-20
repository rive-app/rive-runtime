#include "rive/math/vec2d.hpp"
#include "rive/math/mat2d.hpp"
#include <cmath>

using namespace rive;

Vec2D Vec2D::transformDir(const Vec2D& a, const Mat2D& m) {
    return {
        m[0] * a.x() + m[2] * a.y(),
        m[1] * a.x() + m[3] * a.y(),
    };
}
float Vec2D::length() const {
    return std::sqrt(lengthSquared());
}

Vec2D Vec2D::normalized() const {
    auto len2 = lengthSquared();
    auto scale = len2 > 0 ? (1 / std::sqrt(len2)) : 1;
    return *this * scale;
}

Vec2D Vec2D::lerp(const Vec2D& a, const Vec2D& b, float f) {
    return {
        a.x() + f * (b[0] - a.x()),
        a.y() + f * (b[1] - a.y()),
    };
}

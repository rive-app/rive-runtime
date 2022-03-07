#include "rive/math/aabb.hpp"
#include <cmath>

using namespace rive;

bool AABB::contains(const AABB& a, const AABB& b) {
    return a[0] <= b[0] && a[1] <= b[1] && b[2] <= a[2] && b[3] <= a[3];
}

bool AABB::isValid(const AABB& a) {
    float dx = a[2] - a[0];
    float dy = a[3] - a[1];
    return dx >= 0.0f && dy >= 0.0f && std::isfinite(a[0]) &&
           std::isfinite(a[1]) && std::isfinite(a[2]) && std::isfinite(a[3]);
}

bool AABB::testOverlap(const AABB& a, const AABB& b) {
    float d1x = b[0] - a[2];
    float d1y = b[1] - a[3];

    float d2x = a[0] - b[2];
    float d2y = a[1] - b[3];

    if (d1x > 0.0 || d1y > 0.0) {
        return false;
    }

    if (d2x > 0.0 || d2y > 0.0) {
        return false;
    }

    return true;
}

void AABB::transform(AABB& out, const AABB& a, const Mat2D& matrix) {
    const auto p1 = matrix * Vec2D(a[0], a[1]);
    const auto p2 = matrix * Vec2D(a[2], a[1]);
    const auto p3 = matrix * Vec2D(a[2], a[3]);
    const auto p4 = matrix * Vec2D(a[0], a[3]);

    out[0] = std::fmin(p1[0], std::fmin(p2[0], std::fmin(p3[0], p4[0])));
    out[1] = std::fmin(p1[1], std::fmin(p2[1], std::fmin(p3[1], p4[1])));
    out[2] = std::fmax(p1[0], std::fmax(p2[0], std::fmax(p3[0], p4[0])));
    out[3] = std::fmax(p1[1], std::fmax(p2[1], std::fmax(p3[1], p4[1])));
}

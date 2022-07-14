/*
 * Copyright 2022 Rive
 */

#include "rive/math/raw_path_utils.hpp"
#include <cmath>

// just putting a sane limit, the particular value not important.
constexpr int MAX_LINE_SEGMENTS = 100;

// (a+c)/2 - (a+2b+c)/4)
//  a/4 - b/2 + c/4
// d = |a - 2b + c|/4
// count = sqrt(d / tol)
//
int rive::computeApproximatingQuadLineSegments(const rive::Vec2D pts[3], float invTolerance) {
    auto diff = pts[0] - rive::two(pts[1]) + pts[2];
    float d = diff.length();
    float count = sqrtf(d * invTolerance * 0.25f);
    return std::max(1, std::min((int)std::ceil(count), MAX_LINE_SEGMENTS));
}

int rive::computeApproximatingCubicLineSegments(const rive::Vec2D pts[4], float invTolerance) {
    auto abc = pts[0] - pts[1] - pts[1] + pts[2];
    auto bcd = pts[1] - pts[2] - pts[2] + pts[3];
    float dx = std::max(std::abs(abc.x), std::abs(bcd.x));
    float dy = std::max(std::abs(abc.y), std::abs(bcd.y));
    float d = Vec2D{dx, dy}.length();
    // count = sqrt(3*d / 4*tol)
    float count = sqrtf(d * invTolerance * 0.75f);
    return std::max(1, std::min((int)std::ceil(count), MAX_LINE_SEGMENTS));
}

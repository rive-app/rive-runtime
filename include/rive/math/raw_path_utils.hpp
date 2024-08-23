/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_RAW_PATH_UTILS_HPP_
#define _RIVE_RAW_PATH_UTILS_HPP_

#include "rive/math/vec2d.hpp"

namespace rive
{
static inline Vec2D two(Vec2D v) { return v + v; }

// Caches the setup to evaluate a quadratic bezier. Useful if you
// want to evaluate the save curve at multiple t values.
// clang-format off
    struct EvalQuad {
        const Vec2D a, b, c; // at^2 + bt + c

        // pts are the 3 quadratic bezier control points
        EvalQuad(const Vec2D pts[3]) :
            a(pts[0] - two(pts[1]) + pts[2]),
            b(two(pts[1] - pts[0])),
            c(pts[0]) {}

        Vec2D operator()(float t) const { return (a * t + b) * t + c; }
    };
// clang-format on

// Caches the setup to evaluate a cubic bezier. Useful if you
// want to evaluate the save curve at multiple t values.
struct EvalCubic
{
    const Vec2D a, b, c, d; // at^3 + bt^2 + ct + d

    // pts are the 4 cubic bezier control points
    EvalCubic(const Vec2D pts[4]) :
        a(pts[3] + 3 * (pts[1] - pts[2]) - pts[0]),
        b(3 * (pts[2] - two(pts[1]) + pts[0])),
        c(3 * (pts[1] - pts[0])),
        d(pts[0])
    {}

    Vec2D operator()(float t) const { return ((a * t + b) * t + c) * t + d; }
};

// Extract a subcurve from the curve (given start and end t-values)

extern void quad_subdivide(const Vec2D src[3], float t, Vec2D dst[5]);
extern void cubic_subdivide(const Vec2D src[4], float t, Vec2D dst[7]);

extern void line_extract(const Vec2D src[2], float startT, float endT, Vec2D dst[2]);
extern void quad_extract(const Vec2D src[3], float startT, float endT, Vec2D dst[3]);
extern void cubic_extract(const Vec2D src[4], float startT, float endT, Vec2D dst[4]);

} // namespace rive

#endif

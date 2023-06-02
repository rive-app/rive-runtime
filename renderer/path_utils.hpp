/*
 * Copyright 2021 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Initial import from skia:src/gpu/tessellate/Tessellation.h
 *                     skia:src/core/SkGeometry.h
 *
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/math/vec2d.hpp"
#include <math.h>

namespace rive::pathutils
{
// Decides the number of polar segments the tessellator adds for each curve. (Uniform steps in
// tangent angle.) The tessellator will add this number of polar segments for each radian of
// rotation in local path space.
template <int Precision> float CalcPolarSegmentsPerRadian(float approxDevStrokeRadius)
{
    float cosTheta = 1.f - (1.f / Precision) / approxDevStrokeRadius;
    return .5f / acosf(std::max(cosTheta, -1.f));
}

Vec2D EvalCubicAt(const Vec2D p[4], float t);

// Given a src cubic bezier, chop it at the specified t value,
// where 0 <= t <= 1, and return the two new cubics in dst:
// dst[0..3] and dst[3..6]
void ChopCubicAt(const Vec2D src[4], Vec2D dst[7], float t);

// Given a src cubic bezier, chop it at the specified t0 and t1 values,
// where 0 <= t0 <= t1 <= 1, and return the three new cubics in dst:
// dst[0..3], dst[3..6], and dst[6..9]
void ChopCubicAt(const Vec2D src[4], Vec2D dst[10], float t0, float t1);

// Given a src cubic bezier, chop it at the specified t values,
// where 0 <= t0 <= t1 <= ... <= 1, and return the new cubics in dst:
// dst[0..3],dst[3..6],...,dst[3*t_count..3*(t_count+1)]
void ChopCubicAt(const Vec2D src[4], Vec2D dst[], const float tValues[], int tCount);

// Measures the angle between two vectors, in the range [0, pi].
float MeasureAngleBetweenVectors(Vec2D a, Vec2D b);

// Returns 0, 1, or 2 T values at which to chop the given curve in order to guarantee the resulting
// cubics are convex and rotate no more than 180 degrees.
//
//   - If the cubic is "serpentine", then the T values are any inflection points in [0 < T < 1].
//   - If the cubic is linear, then the T values are any 180-degree cusp points in [0 < T < 1].
//   - Otherwise the T value is the point at which rotation reaches 180 degrees, iff in [0 < T < 1].
//
// 'areCusps' is set to true if the chop point occurred at a cusp (within tolerance), or if the chop
// point(s) occurred at 180-degree turnaround points on a degenerate flat line.
int FindCubicConvex180Chops(const Vec2D[], float T[2], bool* areCusps);

#if 0
// Returns a new path, equivalent to 'path' within the given viewport, whose verbs can all be drawn
// with 'maxSegments' tessellation segments or fewer, while staying within '1/tessellationPrecision'
// pixels of the true curve. Curves and chops that fall completely outside the viewport are
// flattened into lines.
SkPath PreChopPathCurves(float tessellationPrecision,
                         const SkPath&,
                         const SkMatrix&,
                         const SkRect& viewport);
#endif

} // namespace rive::pathutils

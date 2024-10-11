/*
 * Copyright 2020 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "rive/math/math_types.hpp"
#include "rive/renderer/gpu.hpp"
#include <catch.hpp>

namespace rive
{
TEST_CASE("FindTransformedArea", "[pls]")
{
    AABB unitSquare{0, 0, 1, 1};
    CHECK(gpu::FindTransformedArea(unitSquare, Mat2D()) == 1);
    CHECK(gpu::FindTransformedArea(unitSquare, Mat2D::fromScale(2, 2)) == 4);
    CHECK(gpu::FindTransformedArea(unitSquare, Mat2D::fromScale(2, 1)) == 2);
    CHECK(gpu::FindTransformedArea(unitSquare, Mat2D::fromScale(0, 1)) == 0);
    CHECK(gpu::FindTransformedArea(unitSquare,
                                   Mat2D::fromRotation(math::PI / 4)) ==
          Approx(1.f));
    CHECK(gpu::FindTransformedArea(
              unitSquare,
              Mat2D::fromRotation(math::PI / 8).scale({2, 2})) == Approx(4.f));
    CHECK(gpu::FindTransformedArea(
              unitSquare,
              Mat2D::fromRotation(math::PI / 16).scale({2, 1})) == Approx(2.f));
    CHECK(gpu::FindTransformedArea(unitSquare, {1, .87f, 8, 8 * .87f, 0, 0}) ==
          Approx(0.f).margin(math::EPSILON));
}
} // namespace rive

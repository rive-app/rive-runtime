/*
 * Copyright 2021 Google Inc.
 * Copyright 2022 Rive
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/renderer.hpp"
#include "rive/math/math_types.hpp"

using namespace rivegm;
using namespace rive;

constexpr int kSize = 1000;

// Makes sure PathInnerTriangulateOp uses correct stencil settings when there is
// a clip in the stencil buffer.
static void draw_clipped_flower(Renderer* renderer,
                                FillRule fillType,
                                bool asNestedClip)
{
    constexpr static int NUM_PETALS = 9;
    constexpr static float M = kSize / 3.f;
    constexpr static float A = kSize / 2.f;
    Path flower;
    flower->fillRule(fillType);
    flower->moveTo(1 * M + A, 0 * M + A);
    for (int i = 1; i <= NUM_PETALS; ++i)
    {
        float c1 = 2 * rive::math::PI * (i - 2 / 3.f) / NUM_PETALS;
        float c2 = 2 * rive::math::PI * (i - 1 / 3.f) / NUM_PETALS;
        float theta = 2 * rive::math::PI * i / NUM_PETALS;
        flower->cubicTo(cosf(c1) * 1.65 * M + A,
                        sinf(c1) * 1.65 * M + A,
                        cosf(c2) * 1.65 * M + A,
                        sinf(c2) * 1.65 * M + A,
                        cosf(theta) * M + A,
                        sinf(theta) * M + A);
    }
    flower->close();
    path_addOval(
        flower,
        AABB(-.75f * M + A, -.75f * M + A, .75f * M + A, .75f * M + A));

    if (asNestedClip)
    {
        renderer->clipPath(flower);
    }

    PathBuilder clip;
    clip.fillRule(fillType);
    constexpr static int kGridCount = 50;
    constexpr static float kCellSize = (float)kSize / kGridCount;
    for (int y = 0; y < kGridCount; y += fillType == FillRule::evenOdd ? 2 : 1)
    {
        clip.addRect(AABB(0, y * kCellSize, kSize, (y + 1) * kCellSize),
                     (y & 1) ? rivegm::PathDirection::cw
                             : rivegm::PathDirection::ccw);
    }
    for (int x = fillType == FillRule::evenOdd ? 1 : 0; x < kGridCount;
         x += fillType == FillRule::evenOdd ? 2 : 1)
    {
        clip.addRect(AABB(x * kCellSize, 0, (x + 1) * kCellSize, kSize),
                     (x & 1) ? rivegm::PathDirection::cw
                             : rivegm::PathDirection::ccw);
    }
    renderer->clipPath(clip.detach());

    Paint p;
    p->color(0xffff00ff);
    if (asNestedClip)
    {
        renderer->drawPath(PathBuilder::Rect({0, 0, kSize, kSize}), p);
    }
    else
    {
        renderer->drawPath(flower, p);
    }
}

DEF_SIMPLE_GM_WITH_CLEAR_COLOR(largeclippedpath_winding,
                               0xff00ffff,
                               kSize,
                               kSize,
                               renderer)
{
    draw_clipped_flower(renderer, FillRule::nonZero, /*asNestedClip=*/false);
}

DEF_SIMPLE_GM_WITH_CLEAR_COLOR(largeclippedpath_winding_nested,
                               0xff00ffff,
                               kSize,
                               kSize,
                               renderer)
{
    draw_clipped_flower(renderer, FillRule::nonZero, /*asNestedClip=*/true);
}

DEF_SIMPLE_GM_WITH_CLEAR_COLOR(largeclippedpath_evenodd,
                               0xff00ffff,
                               kSize,
                               kSize,
                               renderer)
{
    draw_clipped_flower(renderer, FillRule::evenOdd, /*asNestedClip=*/false);
}

DEF_SIMPLE_GM_WITH_CLEAR_COLOR(largeclippedpath_evenodd_nested,
                               0xff00ffff,
                               kSize,
                               kSize,
                               renderer)
{
    draw_clipped_flower(renderer, FillRule::evenOdd, /*asNestedClip=*/true);
}

DEF_SIMPLE_GM_WITH_CLEAR_COLOR(largeclippedpath_clockwise,
                               0xff00ffff,
                               kSize,
                               kSize,
                               renderer)
{
    draw_clipped_flower(renderer, FillRule::clockwise, /*asNestedClip=*/false);
}

DEF_SIMPLE_GM_WITH_CLEAR_COLOR(largeclippedpath_clockwise_nested,
                               0xff00ffff,
                               kSize,
                               kSize,
                               renderer)
{
    draw_clipped_flower(renderer, FillRule::clockwise, /*asNestedClip=*/true);
}

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

using namespace rivegm;
using namespace rive;

constexpr int kSize = 1000;

// Makes sure PathInnerTriangulateOp uses correct stencil settings when there is
// a clip in the stencil buffer.
static void draw_clipped_flower(Renderer* renderer, FillRule fillType)
{
    PathBuilder clip;
    clip.fillRule(FillRule::nonZero);
    constexpr static int kGridCount = 50;
    constexpr static float kCellSize = (float)kSize / kGridCount;
    for (int y = 0; y < kGridCount; ++y)
    {
        clip.addRect(AABB(0, y * kCellSize, kSize, (y + 1) * kCellSize),
                     (y & 1) ? rivegm::PathDirection::cw
                             : rivegm::PathDirection::ccw);
    }
    for (int x = 0; x < kGridCount; ++x)
    {
        clip.addRect(AABB(x * kCellSize, 0, (x + 1) * kCellSize, kSize),
                     (x & 1) ? rivegm::PathDirection::cw
                             : rivegm::PathDirection::ccw);
    }
    renderer->clipPath(clip.detach());
    Path flower;
    flower->fillRule(fillType);
    flower->moveTo(1, 0);
    constexpr static int kNumPetals = 9;
    for (int i = 1; i <= kNumPetals; ++i)
    {
        float c1 = 2 * M_PI * (i - 2 / 3.f) / kNumPetals;
        float c2 = 2 * M_PI * (i - 1 / 3.f) / kNumPetals;
        float theta = 2 * M_PI * i / kNumPetals;
        flower->cubicTo(cosf(c1) * 1.65,
                        sinf(c1) * 1.65,
                        cosf(c2) * 1.65,
                        sinf(c2) * 1.65,
                        cosf(theta),
                        sinf(theta));
    }
    flower->close();
    path_addOval(flower, AABB(-.75f, -.75f, .75f, .75f));
    renderer->translate(kSize / 2.f, kSize / 2.f);
    renderer->scale(kSize / 3.f, kSize / 3.f);
    Paint p;
    p->color(0xffff00ff);
    renderer->drawPath(flower, p);
}

DEF_SIMPLE_GM_WITH_CLEAR_COLOR(largeclippedpath_winding,
                               0xff00ffff,
                               kSize,
                               kSize,
                               renderer)
{
    draw_clipped_flower(renderer, FillRule::nonZero);
}

DEF_SIMPLE_GM_WITH_CLEAR_COLOR(largeclippedpath_evenodd,
                               0xff00ffff,
                               kSize,
                               kSize,
                               renderer)
{
    draw_clipped_flower(renderer, FillRule::evenOdd);
}

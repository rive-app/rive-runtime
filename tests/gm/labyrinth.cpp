/*
 * Copyright 2019 Google LLC.
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

/**
 * Repro case for https://bugs.chromium.org/p/chromium/issues/detail?id=913223
 *
 * The original bug was filed against square caps, but here we also draw the
 * labyrinth using round and butt caps.
 *
 * Square and round caps expose over-coverage on overlaps when using coverage
 * counting.
 *
 * Butt caps expose under-coverage on abutted strokes when using a 'max()'
 * coverage function.
 */
static void draw_labyrinth(Renderer* renderer, StrokeCap cap)
{
    constexpr static bool kRows[11][12] = {
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 1, 1},
        {0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1},
        {1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0},
        {0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1},
        {1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 0},
        {0, 1, 0, 1, 1, 1, 0, 0, 1, 1, 1, 0},
        {1, 0, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1},
        {0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1},
        {0, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    };

    constexpr static bool kCols[13][10] = {
        {1, 1, 1, 1, 0, 1, 1, 1, 1, 1},
        {0, 0, 1, 0, 0, 0, 1, 1, 1, 0},
        {0, 1, 1, 0, 1, 1, 1, 0, 0, 1},
        {1, 1, 0, 0, 0, 0, 1, 0, 1, 0},
        {0, 0, 1, 0, 1, 0, 0, 0, 0, 1},
        {0, 0, 1, 1, 1, 0, 0, 0, 1, 0},
        {0, 1, 0, 1, 1, 1, 0, 0, 0, 0},
        {1, 1, 1, 0, 1, 1, 1, 0, 1, 0},
        {1, 1, 0, 1, 1, 0, 0, 0, 1, 0},
        {0, 0, 1, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 1, 1, 0, 0, 0, 0, 1, 0},
        {0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
        {1, 1, 1, 1, 1, 1, 0, 1, 1, 1},
    };

    Path maze;
    for (size_t y = 0; y < std::size(kRows); ++y)
    {
        for (size_t x = 0; x < std::size(kRows[0]); ++x)
        {
            if (kRows[y][x])
            {
                maze->moveTo(x, y);
                maze->lineTo(x + 1, y);
            }
        }
    }
    for (size_t x = 0; x < std::size(kCols); ++x)
    {
        for (size_t y = 0; y < std::size(kCols[0]); ++y)
        {
            if (kCols[x][y])
            {
                maze->moveTo(x, y);
                maze->lineTo(x, y + 1);
            }
        }
    }

    Paint paint;
    paint->style(RenderPaintStyle::stroke);
    paint->thickness(.1f);
    paint->color(0xff406060);
    paint->cap(cap);

    renderer->translate(10.5, 10.5);
    renderer->scale(40, 40);
    renderer->drawPath(maze, paint);
}

constexpr static int kWidth = 500;
constexpr static int kHeight = 420;

DEF_SIMPLE_GM(labyrinth_square, kWidth, kHeight, renderer)
{
    draw_labyrinth(renderer, StrokeCap::square);
}

DEF_SIMPLE_GM(labyrinth_round, kWidth, kHeight, renderer)
{
    draw_labyrinth(renderer, StrokeCap::round);
}

DEF_SIMPLE_GM(labyrinth_butt, kWidth, kHeight, renderer)
{
    draw_labyrinth(renderer, StrokeCap::butt);
}

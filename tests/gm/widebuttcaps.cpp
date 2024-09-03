/*
 * Copyright 2020 Google Inc.
 * Copyright 2022 Rive
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/renderer.hpp"
#include "skia/include/utils/SkRandom.h"

using namespace rivegm;
using namespace rive;

static constexpr float kStrokeWidth = 100;
static constexpr int kTestWidth = 120 * 4;
static constexpr int kTestHeight = 120 * 3 + 140;

static void draw_strokes(Renderer* renderer, SkRandom* rand, RenderPath* path, RenderPath* cubic)
{
    Paint strokePaint;
    strokePaint->thickness(kStrokeWidth);
    strokePaint->style(RenderPaintStyle::stroke);

    renderer->save();

    strokePaint->join(StrokeJoin::bevel);
    strokePaint->color(rand->nextU() | 0xff808080);
    renderer->drawPath(path, strokePaint);

    renderer->translate(120, 0);
    strokePaint->join(StrokeJoin::round);
    strokePaint->color(rand->nextU() | 0xff808080);
    renderer->drawPath(path, strokePaint);

    renderer->translate(120, 0);
    strokePaint->join(StrokeJoin::miter);
    strokePaint->color(rand->nextU() | 0xff808080);
    renderer->drawPath(path, strokePaint);

    renderer->translate(120, 0);
    strokePaint->color(rand->nextU() | 0xff808080);
    renderer->drawPath(cubic, strokePaint);

    renderer->restore();
}

class WideButtCapsGM : public GM
{
public:
    WideButtCapsGM() : GM(kTestWidth, kTestHeight, "widebuttcaps") {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* renderer) override
    {
        SkRandom rand;

        renderer->save();

        renderer->translate(60, 60);

        {
            Path path, cubic;
            path->lineTo(10, 0);
            path->lineTo(10, 10);
            cubic->cubicTo(10, 0, 10, 0, 10, 10);
            draw_strokes(renderer, &rand, path, cubic);
        }
        renderer->translate(0, 120);

        {
            Path path, cubic;
            path->lineTo(0, -10);
            path->lineTo(0, 10);
            cubic->cubicTo(0, -10, 0, -10, 0, 10);
            draw_strokes(renderer, &rand, path, cubic);
        }
        renderer->translate(0, 120);

        {
            Path path, cubic;
            path->lineTo(0, -10);
            path->lineTo(10, -10);
            path->lineTo(10, 10);
            path->lineTo(0, 10);
            cubic->cubicTo(0, -10, 10, 10, 0, 10);
            draw_strokes(renderer, &rand, path, cubic);
        }
        renderer->translate(0, 140);

        {
            Path path, cubic;
            path->lineTo(0, -10);
            path->lineTo(10, -10);
            path->lineTo(10, 0);
            path->lineTo(0, 0);
            cubic->cubicTo(0, -10, 10, 0, 0, 0);
            draw_strokes(renderer, &rand, path, cubic);
        }
        renderer->translate(0, 120);

        renderer->restore();
    }
};

GMREGISTER(return new WideButtCapsGM();)

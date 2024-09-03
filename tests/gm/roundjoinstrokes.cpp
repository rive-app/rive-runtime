/*
 * Copyright 2018 Google Inc.
 * Copyright 2022 Rive
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/renderer.hpp"
#include "skia/include/core/SkMatrix.h"
#include "skia/include/core/SkPoint.h"
#include "skia/include/core/SkRect.h"
#include "skia/include/utils/SkRandom.h"

static constexpr float kStrokeWidth = 30;

using namespace rivegm;
using namespace rive;

class RoundJoinStrokesGM : public GM
{
public:
    RoundJoinStrokesGM() : GM(800, 600, "roundjoinstrokes") {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* renderer) override
    {
        SkRandom rand;

        Paint strokePaint;
        strokePaint->style(RenderPaintStyle::stroke);
        strokePaint->thickness(kStrokeWidth);
        strokePaint->cap(StrokeCap::butt);
        strokePaint->join(StrokeJoin::round);

        {
            Path path;
            path->moveTo(30, 170);
            path->lineTo(170, 30);
            strokePaint->color(rand.nextU() | 0xff808080);
            renderer->drawPath(path, strokePaint);
        }

        {
            Path path;
            path->moveTo(230, 170);
            path->lineTo(370, 30);
            path->close();
            strokePaint->color(rand.nextU() | 0xff808080);
            renderer->drawPath(path, strokePaint);
        }

        {
            Path path;
            path->moveTo(570, 170);
            path->lineTo(430, 30);
            path->lineTo(570, 170);
            path->close();
            strokePaint->color(rand.nextU() | 0xff808080);
            renderer->drawPath(path, strokePaint);
        }

        {
            Path path;
            path->moveTo(770, 170);
            path->lineTo(630, 30);
            path->lineTo(770, 170);
            strokePaint->color(rand.nextU() | 0xff808080);
            renderer->drawPath(path, strokePaint);
        }

        {
            Path path;
            path->moveTo(30, 370);
            path->cubicTo(230, 230, 230, 230, 30, 370);
            strokePaint->color(rand.nextU() | 0xff808080);
            renderer->drawPath(path, strokePaint);
        }

        {
            Path path;
            path->moveTo(770, 370);
            path->cubicTo(570, 230, 570, 230, 770, 370);
            path->close();
            strokePaint->color(rand.nextU() | 0xff808080);
            renderer->drawPath(path, strokePaint);
        }

        {
            Path path;
            path->moveTo(400, 550);
            path->cubicTo(-100, 130, 900, 130, 400, 550);
            path->close();
            strokePaint->color(rand.nextU() | 0xff808080);
            renderer->drawPath(path, strokePaint);
        }

        strokePaint->cap(StrokeCap::round);
        {
            Path path;
            path->moveTo(100, 500);
            strokePaint->color(rand.nextU() | 0xff808080);
            renderer->drawPath(path, strokePaint);
        }

        {
            Path path;
            path->moveTo(700, 500);
            path->close();
            strokePaint->color(rand.nextU() | 0xff808080);
            renderer->drawPath(path, strokePaint);
        }
    }
};

GMREGISTER(return new RoundJoinStrokesGM())

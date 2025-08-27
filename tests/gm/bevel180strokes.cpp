/*
 * Copyright 2018 Google Inc.
 * Copyright 2022 Rive
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/math/math_types.hpp"
#include "rive/renderer.hpp"
#include "common/rand.hpp"

using namespace rivegm;
using namespace rive;

class Bevel180StrokesGM : public GM
{
public:
    Bevel180StrokesGM() : GM(300, 300) {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* renderer) override
    {
        Rand rand;

        const float r = 141.5;

        Paint strokePaint;
        strokePaint->style(RenderPaintStyle::stroke);
        strokePaint->thickness(15 / r);
        strokePaint->cap(StrokeCap::butt);
        strokePaint->join(StrokeJoin::bevel);

        renderer->translate(150, 150);
        renderer->scale(r, r);

        const size_t spokes = 20;
        for (size_t i = 0; i < spokes; ++i)
        {
            float theta = math::PI * 2 * i / spokes;
            Path path;
            if (i == 0)
                path->lineTo(1, 0);
            else if (i == spokes / 4)
                path->lineTo(0, 1);
            else if (i == spokes * 2 / 4)
                path->lineTo(-1, 0);
            else if (i == spokes * 3 / 4)
                path->lineTo(0, -1);
            else
                path->lineTo(cosf(theta), sinf(theta));
            path->lineTo(0, 0);
            strokePaint->color(rand.u32() | 0xff808080);
            renderer->drawPath(path, strokePaint);
        }
    }
};

GMREGISTER(bevel180strokes, return new Bevel180StrokesGM())

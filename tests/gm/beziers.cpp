/*
 * Copyright 2014 Google Inc.
 * Copyright 2022 Rive
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "include/utils/SkRandom.h"
#include "rive/renderer.hpp"

using namespace rivegm;
using namespace rive;

#define W 400
#define H 400
#define N 10

constexpr SkScalar SH = SkIntToScalar(H);

static Path rnd_quad(RenderPaint* paint, SkRandom& rand)
{
    auto a = rand.nextRangeScalar(0, W), b = rand.nextRangeScalar(0, H);

    PathBuilder builder;
    builder.moveTo(a, b);
    for (int x = 0; x < 2; ++x)
    {
        auto c = rand.nextRangeScalar(W / 4.f, W), d = rand.nextRangeScalar(0, H),
             e = rand.nextRangeScalar(0, W), f = rand.nextRangeScalar(H / 4.f, H);
        builder.quadTo(c, d, e, f);
    }
    paint->color(rand.nextU());
    SkScalar width = rand.nextRangeScalar(1, 5);
    width *= width;
    paint->thickness(width);
    return builder.detach();
}

static Path rnd_cubic(RenderPaint* paint, SkRandom& rand)
{
    auto a = rand.nextRangeScalar(0, W), b = rand.nextRangeScalar(0, H);

    PathBuilder builder;
    builder.moveTo(a, b);
    for (int x = 0; x < 2; ++x)
    {
        auto c = rand.nextRangeScalar(W / 4.f, W), d = rand.nextRangeScalar(0, H),
             e = rand.nextRangeScalar(0, W), f = rand.nextRangeScalar(H / 4.f, H),
             g = rand.nextRangeScalar(W / 4.f, W), h = rand.nextRangeScalar(H / 4.f, H);
        builder.cubicTo(c, d, e, f, g, h);
    }
    paint->color(rand.nextU());
    SkScalar width = rand.nextRangeScalar(1, 5);
    width *= width;
    paint->thickness(width);
    return builder.detach();
}

class BeziersGM : public GM
{
public:
    BeziersGM() : GM(W, H * 2, "beziers") {}

protected:
    void onDraw(Renderer* canvas) override
    {
        Paint paint;
        paint->style(RenderPaintStyle::stroke);
        paint->thickness(SkIntToScalar(9) / 2);

        SkRandom rand;
        for (int i = 0; i < N; i++)
        {
            canvas->drawPath(rnd_quad(paint, rand), paint);
        }
        canvas->translate(0, SH);
        for (int i = 0; i < N; i++)
        {
            canvas->drawPath(rnd_cubic(paint, rand), paint);
        }
    }
};

GMREGISTER(return new BeziersGM;)

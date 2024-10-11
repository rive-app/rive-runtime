/*
 * Copyright 2014 Google Inc.
 * Copyright 2022 Rive
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "common/rand.hpp"

using namespace rivegm;
using namespace rive;

#define W 400
#define H 400
#define N 10

constexpr float SH = H;

static Path rnd_quad(RenderPaint* paint, Rand& rand)
{
    auto a = rand.f32(0, W), b = rand.f32(0, H);

    PathBuilder builder;
    builder.moveTo(a, b);
    for (int x = 0; x < 2; ++x)
    {
        auto c = rand.f32(W / 4.f, W), d = rand.f32(0, H), e = rand.f32(0, W),
             f = rand.f32(H / 4.f, H);
        builder.quadTo(c, d, e, f);
    }
    paint->color(rand.u32());
    float width = rand.f32(1, 5);
    width *= width;
    paint->thickness(width);
    return builder.detach();
}

static Path rnd_cubic(RenderPaint* paint, Rand& rand)
{
    auto a = rand.f32(0, W), b = rand.f32(0, H);

    PathBuilder builder;
    builder.moveTo(a, b);
    for (int x = 0; x < 2; ++x)
    {
        auto c = rand.f32(W / 4.f, W), d = rand.f32(0, H), e = rand.f32(0, W),
             f = rand.f32(H / 4.f, H), g = rand.f32(W / 4.f, W),
             h = rand.f32(H / 4.f, H);
        builder.cubicTo(c, d, e, f, g, h);
    }
    paint->color(rand.u32());
    float width = rand.f32(1, 5);
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
        paint->thickness(9.0f / 2.0f);

        Rand rand;
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

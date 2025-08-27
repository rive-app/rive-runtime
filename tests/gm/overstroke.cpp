/*
 * Copyright 2016 Google Inc.
 * Copyright 2022 Rive
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This GM exercises stroking of paths with large stroke lengths, which is
 * referred to as "overstroke" for brevity. In Skia as of 8/2016 we offset
 * each part of the curve the request amount even if it makes the offsets
 * overlap and create holes. There is not a really great algorithm for this
 * and several other 2D graphics engines have the same bug.
 *
 * The old Nvidia Path Renderer used to yield correct results, so a possible
 * direction of attack is to use the GPU and a completely different algorithm.
 *
 * See crbug.com/589769 skbug.com/5405 skbug.com/5406
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/renderer.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/math/contour_measure.hpp"

using namespace rivegm;
using namespace rive;

const float OVERSTROKE_WIDTH = 500.0f;
const float NORMALSTROKE_WIDTH = 3.0f;

//////// path and paint builders

Paint make_normal_paint()
{
    Paint p;
    p->style(RenderPaintStyle::stroke);
    p->thickness(NORMALSTROKE_WIDTH);
    p->color(0xff0000ff);

    return p;
}

Paint make_overstroke_paint()
{
    Paint p;
    p->style(RenderPaintStyle::stroke);
    p->thickness(OVERSTROKE_WIDTH);

    return p;
}

RawPath quad_path()
{
    RawPath path;
    path.moveTo(0, 0);
    path.lineTo(100, 0);
    path.quadTo(50, -40, 0, 0);
    path.close();
    return path;
}

RawPath cubic_path()
{
    RawPath path;
    path.moveTo(0, 0);
    path.cubicTo(25, 75, 75, -50, 100, 0);
    return path;
}

RawPath oval_path()
{
    RawPath path;
    path.addOval({0, -25, 100, 25});
    return path;
}

Path ribs_path(const RawPath& path, float radius)
{
    Path ribs;

    const float spacing = 5.0f;
    float accum = 0.0f;

    ContourMeasureIter iter(&path);
    while (auto meas = iter.next())
    {
        while (accum < meas->length())
        {
            auto posTan = meas->getPosTan(accum);
            Vec2D pos = posTan.pos;
            // there appeara to be a bug somewhere that is not normalizing this
            // when it should, so i am doing it here
            Vec2D tan = posTan.tan.normalized();
            tan = {tan.y * radius, -tan.x * radius};

            Vec2D start = pos + tan;
            Vec2D end = pos - tan;

            ribs->moveTo(start.x, start.y);
            ribs->lineTo(end.x, end.y);

            accum += spacing;
        }
        accum += meas->length();
    }

    /*
    SkPathMeasure meas(skpath, false);
    SkScalar length = meas.getLength();
    SkPoint pos;
    SkVector tan;
    while (accum < length)
    {
        if (meas.getPosTan(accum, &pos, &tan))
        {
            tan.scale(radius);
            tan = {tan.y(), -tan.x()};

            ribs->moveTo(pos.x() + tan.x(), pos.y() + tan.y());
            ribs->lineTo(pos.x() - tan.x(), pos.y() - tan.y());
        }
        accum += spacing;
    }
    */

    return ribs;
}

void draw_ribs(Renderer* canvas, const RawPath& path)
{
    Path ribs = ribs_path(path, OVERSTROKE_WIDTH / 2.0f);
    Paint p = make_normal_paint();
    p->thickness(1);
    p->color(0xff00ff00);

    canvas->drawPath(ribs, p);
}

///////// quads

void draw_small_quad(Renderer* canvas)
{
    // scaled so it's visible
    // canvas->scale(8, 8);

    Paint p = make_normal_paint();
    RawPath path = quad_path();

    auto renderPath = renderPathFromRawPath(path);

    draw_ribs(canvas, path);
    canvas->drawPath(renderPath.get(), p);
}

void draw_large_quad(Renderer* canvas)
{
    Paint p = make_overstroke_paint();
    RawPath path = quad_path();

    auto renderPath = renderPathFromRawPath(path);

    canvas->drawPath(renderPath.get(), p);
    draw_ribs(canvas, path);
}

void draw_stroked_quad(Renderer* canvas)
{
    canvas->translate(400, 0);
    draw_large_quad(canvas);
}

////////// cubics

void draw_small_cubic(Renderer* canvas)
{
    Paint p = make_normal_paint();
    RawPath path = cubic_path();

    auto renderPath = renderPathFromRawPath(path);

    draw_ribs(canvas, path);
    canvas->drawPath(renderPath.get(), p);
}

void draw_large_cubic(Renderer* canvas)
{
    Paint p = make_overstroke_paint();
    RawPath path = cubic_path();

    auto renderPath = renderPathFromRawPath(path);

    canvas->drawPath(renderPath.get(), p);
    draw_ribs(canvas, path);
}

void draw_stroked_cubic(Renderer* canvas)
{
    canvas->translate(400, 0);
    draw_large_cubic(canvas);
}

////////// ovals

void draw_small_oval(Renderer* canvas)
{
    Paint p = make_normal_paint();
    RawPath path = oval_path();

    auto renderPath = renderPathFromRawPath(path);

    draw_ribs(canvas, path);
    canvas->drawPath(renderPath.get(), p);
}

void draw_large_oval(Renderer* canvas)
{
    Paint p = make_overstroke_paint();
    RawPath path = oval_path();

    auto renderPath = renderPathFromRawPath(path);

    canvas->drawPath(renderPath.get(), p);
    draw_ribs(canvas, path);
}

void draw_stroked_oval(Renderer* canvas)
{
    canvas->translate(400, 0);
    draw_large_oval(canvas);
}

////////// gm

void (*examples[])(Renderer* canvas) = {
    draw_small_quad,
    draw_stroked_quad,
    draw_small_cubic,
    draw_stroked_cubic,
    draw_small_oval,
    draw_stroked_oval,
};

DEF_SIMPLE_GM(OverStroke, 500, 500, canvas)
{
    const size_t length = sizeof(examples) / sizeof(examples[0]);
    const size_t width = 2;

    for (size_t i = 0; i < length; i++)
    {
        int x = (int)(i % width);
        int y = (int)(i / width);

        canvas->save();
        canvas->translate(150.0f * x, 150.0f * y);
        canvas->scale(0.2f, 0.2f);
        canvas->translate(300.0f, 400.0f);

        examples[i](canvas);

        canvas->restore();
    }
}

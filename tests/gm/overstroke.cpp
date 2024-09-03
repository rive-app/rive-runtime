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
#include "skia/include/core/SkColor.h"
#include "skia/include/core/SkMatrix.h"
#include "skia/include/core/SkScalar.h"
#include "skia/include/core/SkPathBuilder.h"
#include "skia/include/core/SkPathMeasure.h"

using namespace rivegm;
using namespace rive;

const SkScalar OVERSTROKE_WIDTH = 500.0f;
const SkScalar NORMALSTROKE_WIDTH = 3.0f;

//////// path and paint builders

Paint make_normal_paint()
{
    Paint p;
    p->style(RenderPaintStyle::stroke);
    p->thickness(NORMALSTROKE_WIDTH);
    p->color(SK_ColorBLUE);

    return p;
}

Paint make_overstroke_paint()
{
    Paint p;
    p->style(RenderPaintStyle::stroke);
    p->thickness(OVERSTROKE_WIDTH);

    return p;
}

Path quad_path(SkPath* skpath)
{
    *skpath = SkPathBuilder().moveTo(0, 0).lineTo(100, 0).quadTo(50, -40, 0, 0).close().detach();
    return PathBuilder().moveTo(0, 0).lineTo(100, 0).quadTo(50, -40, 0, 0).close().detach();
}

Path cubic_path(SkPath* skpath)
{
    skpath->moveTo(0, 0);
    skpath->cubicTo(25, 75, 75, -50, 100, 0);

    Path path;
    path->moveTo(0, 0);
    path->cubicTo(25, 75, 75, -50, 100, 0);

    return path;
}

Path oval_path(SkPath* skpath)
{
    *skpath = SkPathBuilder().arcTo({0, -25, 100, 25}, 0, 359, true).close().detach();
    return PathBuilder().addOval({0, -25, 100, 25}).close().detach();
}

Path ribs_path(const SkPath& skpath, SkScalar radius)
{
    Path ribs;

    const SkScalar spacing = 5.0f;
    float accum = 0.0f;

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

    return ribs;
}

void draw_ribs(Renderer* canvas, const SkPath& skpath)
{
    Path ribs = ribs_path(skpath, OVERSTROKE_WIDTH / 2.0f);
    Paint p = make_normal_paint();
    p->thickness(1);
    p->color(SK_ColorGREEN);

    canvas->drawPath(ribs, p);
}

///////// quads

void draw_small_quad(Renderer* canvas)
{
    // scaled so it's visible
    // canvas->scale(8, 8);

    Paint p = make_normal_paint();
    SkPath skpath;
    Path path = quad_path(&skpath);

    draw_ribs(canvas, skpath);
    canvas->drawPath(path, p);
}

void draw_large_quad(Renderer* canvas)
{
    Paint p = make_overstroke_paint();
    SkPath skpath;
    Path path = quad_path(&skpath);

    canvas->drawPath(path, p);
    draw_ribs(canvas, skpath);
}

#if 0
void draw_quad_fillpath(Renderer* canvas) {
    Path path = quad_path();
    Paint p = make_overstroke_paint();

    Paint fillp = make_normal_paint();
    fillp->color(SK_ColorMAGENTA);

    Path fillpath;
    p.getFillPath(path, &fillpath);

    canvas->drawPath(fillpath, fillp);
}
#endif

void draw_stroked_quad(Renderer* canvas)
{
    canvas->translate(400, 0);
    draw_large_quad(canvas);
#if 0
    draw_quad_fillpath(canvas);
#endif
}

////////// cubics

void draw_small_cubic(Renderer* canvas)
{
    Paint p = make_normal_paint();
    SkPath skpath;
    Path path = cubic_path(&skpath);

    draw_ribs(canvas, skpath);
    canvas->drawPath(path, p);
}

void draw_large_cubic(Renderer* canvas)
{
    Paint p = make_overstroke_paint();
    SkPath skpath;
    Path path = cubic_path(&skpath);

    canvas->drawPath(path, p);
    draw_ribs(canvas, skpath);
}

#if 0
void draw_cubic_fillpath(SkCanvas* canvas) {
    Path path = cubic_path();
    Paint p = make_overstroke_paint();

    SkPaint fillp = make_normal_paint();
    fillp.setColor(SK_ColorMAGENTA);

    SkPath fillpath;
    p.getFillPath(path, &fillpath);

    canvas->drawPath(fillpath, fillp);
}
#endif

void draw_stroked_cubic(Renderer* canvas)
{
    canvas->translate(400, 0);
    draw_large_cubic(canvas);
#if 0
    draw_cubic_fillpath(canvas);
#endif
}

////////// ovals

void draw_small_oval(Renderer* canvas)
{
    Paint p = make_normal_paint();

    SkPath skpath;
    Path path = oval_path(&skpath);

    draw_ribs(canvas, skpath);
    canvas->drawPath(path, p);
}

void draw_large_oval(Renderer* canvas)
{
    Paint p = make_overstroke_paint();
    SkPath skpath;
    Path path = oval_path(&skpath);

    canvas->drawPath(path, p);
    draw_ribs(canvas, skpath);
}

#if 0
void draw_oval_fillpath(SkCanvas* canvas) {
    Path path = oval_path();
    Paint p = make_overstroke_paint();

    SkPaint fillp = make_normal_paint();
    fillp.setColor(SK_ColorMAGENTA);

    SkPath fillpath;
    p.getFillPath(path, &fillpath);

    canvas->drawPath(fillpath, fillp);
}
#endif

void draw_stroked_oval(Renderer* canvas)
{
    canvas->translate(400, 0);
    draw_large_oval(canvas);
#if 0
    draw_oval_fillpath(canvas);
#endif
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

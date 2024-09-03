/*
 * Copyright 2011 Google Inc.
 * Copyright 2022 Rive
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/renderer.hpp"
#include "rive/math/math_types.hpp"
#include "skia/include/core/SkColor.h"
#include "skia/include/core/SkPoint.h"
#include "skia/include/utils/SkRandom.h"

using namespace rivegm;
using namespace rive;

#define W 400
#define H 400
#define N 50

constexpr SkScalar SW = SkIntToScalar(W);
constexpr SkScalar SH = SkIntToScalar(H);

static void rnd_rect(AABB* r, RenderPaint* paint, SkRandom& rand)
{
    SkScalar x = rand.nextUScalar1() * W;
    SkScalar y = rand.nextUScalar1() * H;
    SkScalar w = rand.nextUScalar1() * (W >> 2);
    SkScalar h = rand.nextUScalar1() * (H >> 2);
    SkScalar hoffset = rand.nextSScalar1();
    SkScalar woffset = rand.nextSScalar1();

    r->minX = x;
    r->minY = y;
    r->maxX = x + w;
    r->maxY = y + h;
    r->offset(-w / 2 + woffset, -h / 2 + hoffset);

    paint->color(rand.nextU());
    // paint->setAlphaf(1.0f);
}

class StrokesGM : public GM
{
public:
    StrokesGM() : GM(W, H, "strokes_round") {}

protected:
    void onDraw(Renderer* renderer) override
    {
        Paint paint;
        paint->style(RenderPaintStyle::stroke);
        paint->thickness(SkIntToScalar(9) / 2);

        renderer->save();
        renderer->clipPath(PathBuilder::Rect(
            {SkIntToScalar(2), SkIntToScalar(2), SW - SkIntToScalar(2), SH - SkIntToScalar(2)}));

        SkRandom rand;
        for (int i = 0; i < N; i++)
        {
            AABB r;
            rnd_rect(&r, paint, rand);
            renderer->drawPath(PathBuilder::Oval(r), paint);
            rnd_rect(&r, paint, rand);
            renderer->drawPath(PathBuilder::RRect(r, r.width() / 4, r.height() / 4), paint);
            rnd_rect(&r, paint, rand);
        }
        renderer->restore();
    }
};

/* See
   https://code.google.com/p/chromium/issues/detail?id=422974          and
   http://jsfiddle.net/1xnku3sg/2/
 */
class ZeroLenStrokesGM : public GM
{
public:
    ZeroLenStrokesGM() : GM(W * 3 / 7, H * 3 / 4, "zeroPath") {}

private:
    Path fMoveHfPath, fMoveZfPath, /* fDashedfPath, */ fRefPath[4];
    Path fCubicPath, fQuadPath, fLinePath;

protected:
    void onOnceBeforeDraw() override
    {
        // SkAssertResult(SkParsePath::FromSVGString("M0,0h0M10,0h0M20,0h0", &fMoveHfPath));
        // SkAssertResult(SkParsePath::FromSVGString("M0,0zM10,0zM20,0z", &fMoveZfPath));
        // SkAssertResult(SkParsePath::FromSVGString("M0,0h25", &fDashedfPath));
        // SkAssertResult(SkParsePath::FromSVGString("M 0 0 C 0 0 0 0 0 0", &fCubicPath));
        // SkAssertResult(SkParsePath::FromSVGString("M 0 0 Q 0 0 0 0", &fQuadPath));
        // SkAssertResult(SkParsePath::FromSVGString("M 0 0 L 0 0", &fLinePath));
        fMoveHfPath = PathBuilder()
                          .moveTo(0, 0)
                          .lineTo(0, 0)
                          .moveTo(10, 0)
                          .lineTo(10, 0)
                          .moveTo(20, 0)
                          .lineTo(20, 0)
                          .detach();
        fMoveZfPath =
            PathBuilder().moveTo(0, 0).close().moveTo(10, 0).close().moveTo(20, 0).close().detach();
        fCubicPath = PathBuilder().moveTo(0, 0).cubicTo(0, 0, 0, 0, 0, 0).detach();
        fQuadPath = PathBuilder().moveTo(0, 0).quadTo(0, 0, 0, 0).detach();
        fLinePath = PathBuilder().moveTo(0, 0).lineTo(0, 0).detach();

        PathBuilder b[4];
        for (int i = 0; i < 3; ++i)
        {
            b[0].addOval({i * 10.f - 5, 0 - 5, i * 10.f + 5, 0 + 5});
            b[1].addOval({i * 10.f - 10, 0 - 10, i * 10.f + 10, 0 + 10});
            b[2].addRect({i * 10.f - 4, -2, i * 10.f + 4, 6});
            b[3].addRect({i * 10.f - 10, -10, i * 10.f + 10, 10});
        }
        for (int i = 0; i < 4; ++i)
        {
            fRefPath[i] = b[i].detach();
        }
    }

    void onDraw(Renderer* renderer) override
    {
        Paint fillPaint, strokePaint; // , dashPaint;
        strokePaint->style(RenderPaintStyle::stroke);
        StrokeCap caps[3] = {StrokeCap::round, StrokeCap::square, StrokeCap::butt};
        for (int i = 0; i < 3; ++i)
        {
            fillPaint->color(0xff000000);
            strokePaint->color(0xff000000);
            strokePaint->thickness(i ? 8.f : 10.f);
            strokePaint->cap(caps[i]);
            renderer->save();
            renderer->translate(10 + i * 100.f, 10);
            renderer->drawPath(fMoveHfPath, strokePaint);
            renderer->translate(0, 20);
            renderer->drawPath(fMoveZfPath, strokePaint);
#if 0
            dashPaint = strokePaint;
            const SkScalar intervals[] = {0, 10};
            dashPaint.setPathEffect(SkDashPathEffect::Make(intervals, 2, 0));
            SkPath fillPath;
            dashPaint.getFillPath(fDashedfPath, &fillPath);
            canvas->translate(0, 20);
            canvas->drawPath(fDashedfPath, dashPaint);
#endif
            renderer->translate(0, 20);
            renderer->drawPath(fRefPath[i * 2], fillPaint);
            strokePaint->thickness(20);
            strokePaint->color(0x80000000);
            renderer->translate(0, 50);
            renderer->drawPath(fMoveHfPath, strokePaint);
            renderer->translate(0, 30);
            renderer->drawPath(fMoveZfPath, strokePaint);
            renderer->translate(0, 30);
            fillPaint->color(0x80000000);
            renderer->drawPath(fRefPath[1 + i * 2], fillPaint);
            renderer->translate(0, 30);
            renderer->drawPath(fCubicPath, strokePaint);
            renderer->translate(0, 30);
            renderer->drawPath(fQuadPath, strokePaint);
            renderer->translate(0, 30);
            renderer->drawPath(fLinePath, strokePaint);
            renderer->restore();
        }
    }
};

class TeenyStrokesGM : public GM
{
public:
    TeenyStrokesGM() : GM(W, H * .5, "teenyStrokes") {}

private:
    static void line(SkScalar scale, Renderer* canvas, ColorInt color)
    {
        Paint p;
        p->style(RenderPaintStyle::stroke);
        p->color(color);
        canvas->translate(50, 0);
        canvas->save();
        p->thickness(scale * 5);
        canvas->scale(1 / scale, 1 / scale);
        // canvas->drawLine(20 * scale, 20 * scale, 20 * scale, 100 * scale, p);
        // canvas->drawLine(20 * scale, 20 * scale, 100 * scale, 100 * scale, p);
        canvas->drawPath(
            PathBuilder().moveTo(20 * scale, 20 * scale).lineTo(20 * scale, 100 * scale).detach(),
            p);
        canvas->drawPath(
            PathBuilder().moveTo(20 * scale, 20 * scale).lineTo(100 * scale, 100 * scale).detach(),
            p);
        canvas->restore();
    }

    void onDraw(Renderer* canvas) override
    {
        line(0.00005f, canvas, SK_ColorBLACK);
        line(0.000045f, canvas, SK_ColorRED);
        line(0.0000035f, canvas, SK_ColorGREEN);
        line(0.000003f, canvas, SK_ColorBLUE);
        line(0.000002f, canvas, SK_ColorBLACK);
    }
};

DEF_SIMPLE_GM(CubicStroke, 384, 384, canvas)
{
    Paint p;
    p->style(RenderPaintStyle::stroke);
    p->thickness(1.0720f);
    Path path;
    path->moveTo(-6000, -6000);
    path->cubicTo(-3500, 5500, -500, 5500, 2500, -6500);
    canvas->drawPath(path, p);
    p->thickness(1.0721f);
    canvas->translate(10, 10);
    canvas->drawPath(path, p);
    p->thickness(1.0722f);
    canvas->translate(10, 10);
    canvas->drawPath(path, p);
}

DEF_SIMPLE_GM(zerolinestroke, 90, 120, canvas)
{
    Paint paint;
    paint->style(RenderPaintStyle::stroke);
    paint->thickness(20);
    paint->cap(StrokeCap::round);

    Path path;
    path->moveTo(30, 90);
    path->lineTo(30, 90);
    path->lineTo(60, 90);
    path->lineTo(60, 90);
    canvas->drawPath(path, paint);

    path = Path();
    path->moveTo(30, 30);
    path->lineTo(60, 30);
    canvas->drawPath(path, paint);

    path = Path();
    path->moveTo(30, 60);
    path->lineTo(30, 60);
    path->lineTo(60, 60);
    canvas->drawPath(path, paint);
}

DEF_SIMPLE_GM(quadcap, 70, 30, canvas)
{
    canvas->translate(-100, 5);

    Paint p;
    p->style(RenderPaintStyle::stroke);
    p->thickness(1);
    PathBuilder b;
    SkPoint pts[] = {{105.738571f, 13.126318f}, {105.738571f, 13.126318f}, {123.753784f, 1.f}};
    SkVector tangent = pts[1] - pts[2];
    tangent.normalize();
    SkPoint pts2[3];
    memcpy(pts2, pts, sizeof(pts));
    const SkScalar capOutset = SK_ScalarPI / 8;
    pts2[0].fX += tangent.fX * capOutset;
    pts2[0].fY += tangent.fY * capOutset;
    pts2[1].fX += tangent.fX * capOutset;
    pts2[1].fY += tangent.fY * capOutset;
    pts2[2].fX += -tangent.fX * capOutset;
    pts2[2].fY += -tangent.fY * capOutset;
    b.moveTo(pts2[0].x(), pts2[0].y());
    b.quadTo(pts2[1].x(), pts2[1].y(), pts2[2].x(), pts2[2].y());
    canvas->drawPath(b.detach(), p);

    b.moveTo(pts[0].x(), pts[0].y());
    b.quadTo(pts[1].x(), pts[1].y(), pts[2].x(), pts[2].y());
    p->cap(StrokeCap::round);
    canvas->translate(30, 0);
    canvas->drawPath(b.detach(), p);
}

class Strokes2GM : public GM
{
public:
    Strokes2GM() : GM(W, H, "strokes_poly") {}

private:
    Path fPath;

protected:
    void onOnceBeforeDraw() override
    {
        SkRandom rand;
        fPath->moveTo(0, 0);
        for (int i = 0; i < 13; i++)
        {
            SkScalar x = rand.nextUScalar1() * (W >> 1);
            SkScalar y = rand.nextUScalar1() * (H >> 1);
            fPath->lineTo(x, y);
        }
    }

    void onDraw(Renderer* canvas) override
    {
        Paint paint;
        paint->style(RenderPaintStyle::stroke);
        paint->thickness(SkIntToScalar(9) / 2);

        canvas->save();
        canvas->clipPath(PathBuilder::Rect(
            {SkIntToScalar(2), SkIntToScalar(2), SW - SkIntToScalar(2), SH - SkIntToScalar(2)}));

        SkRandom rand;
        for (int i = 0; i < N / 2; i++)
        {
            AABB r;
            rnd_rect(&r, paint, rand);
            canvas->translate(SW / 2, SH / 2);
            canvas->transform(Mat2D::fromRotation(15 * math::PI / 180));
            canvas->translate(-SW / 2, -SH / 2);
            canvas->drawPath(fPath, paint);
        }
        canvas->restore();
    }
};

//////////////////////////////////////////////////////////////////////////////

static AABB inset(AABB r)
{
    r.inset(r.width() / 10, r.height() / 10);
    return r;
}

class Strokes3GM : public GM
{
    static void make0(PathBuilder* path, const AABB& bounds)
    {
        path->addRect(bounds, rivegm::PathDirection::cw);
        path->addRect(inset(bounds), rivegm::PathDirection::cw);
    }

    static void make1(PathBuilder* path, const AABB& bounds)
    {
        path->addRect(bounds, rivegm::PathDirection::cw);
        path->addRect(inset(bounds), rivegm::PathDirection::ccw);
    }

    static void make2(PathBuilder* path, const AABB& bounds)
    {
        path->addOval(bounds, rivegm::PathDirection::cw);
        path->addOval(inset(bounds), rivegm::PathDirection::cw);
    }

    static void make3(PathBuilder* path, const AABB& bounds)
    {
        path->addOval(bounds, rivegm::PathDirection::cw);
        path->addOval(inset(bounds), rivegm::PathDirection::ccw);
    }

    static void make4(PathBuilder* path, const AABB& bounds)
    {
        path->addRect(bounds, rivegm::PathDirection::cw);
        AABB r = bounds;
        r.inset(bounds.width() / 10, -bounds.height() / 10);
        path->addOval(r, rivegm::PathDirection::cw);
    }

    static void make5(PathBuilder* path, const AABB& bounds)
    {
        path->addRect(bounds, rivegm::PathDirection::cw);
        AABB r = bounds;
        r.inset(bounds.width() / 10, -bounds.height() / 10);
        path->addOval(r, rivegm::PathDirection::ccw);
    }

public:
    Strokes3GM() : GM(1500, 1500, "strokes3") {}

protected:
    void onDraw(Renderer* canvas) override
    {
        Paint origPaint;
        origPaint->style(RenderPaintStyle::stroke);
        // Paint fillPaint;
        // fillPaint->color(SK_ColorRED);
        Paint strokePaint;
        strokePaint->style(RenderPaintStyle::stroke);
        strokePaint->color(0xFF4444FF);

        void (*procs[])(PathBuilder*, const AABB&) = {make0, make1, make2, make3, make4, make5};

        canvas->translate(SkIntToScalar(20), SkIntToScalar(80));

        AABB bounds = {0, 0, SkIntToScalar(50), SkIntToScalar(50)};
        SkScalar dx = bounds.width() * 4 / 3;
        SkScalar dy = bounds.height() * 5;

        for (size_t i = 0; i < SK_ARRAY_COUNT(procs); ++i)
        {
            PathBuilder orig;
            procs[i](&orig, bounds);
            Path orig2 = orig.detach();

            canvas->save();
            for (int j = 0; j < 13; ++j)
            {
                float thickness = SK_Scalar1 * j * j;
                strokePaint->thickness(thickness);
                canvas->drawPath(orig2, strokePaint);
                canvas->drawPath(orig2, origPaint);
                // Path fill;
                // strokePaint.getFillPath(orig, &fill);
                // canvas->drawPath(fill, fillPaint);
                canvas->translate(dx + thickness, 0);
            }
            canvas->restore();
            canvas->translate(0, dy);
        }
    }
};

class Strokes4GM : public GM
{
public:
    Strokes4GM() : GM(W, H / 4, "strokes_zoomed") {}

protected:
    void onDraw(Renderer* canvas) override
    {
        Paint paint;
        paint->style(RenderPaintStyle::stroke);
        paint->thickness(0.055f);

        canvas->scale(1000, 1000);
        canvas->drawPath(PathBuilder::Oval({-1.97f, 2 - 1.97f, 1.97f, 2 + 1.97f}), paint);
    }
};

// Test stroking for curves that produce degenerate tangents when t is 0 or 1 (see bug 4191)
class Strokes5GM : public GM
{
public:
    Strokes5GM() : GM(W, H, "zero_control_stroke") {}

protected:
    void onDraw(Renderer* canvas) override
    {
        Paint p;
        p->color(SK_ColorRED);
        p->style(RenderPaintStyle::stroke);
        p->thickness(40);
        p->cap(StrokeCap::butt);

        PathBuilder path;
        path.moveTo(157.474f, 111.753f);
        path.cubicTo(128.5f, 111.5f, 35.5f, 29.5f, 35.5f, 29.5f);
        canvas->drawPath(path.detach(), p);
        path.moveTo(250, 50);
        path.quadTo(280, 80, 280, 80);
        canvas->drawPath(path.detach(), p);
        path.moveTo(150, 50);
        path.quadTo(180, 80, 180, 80);
        canvas->drawPath(path.detach(), p);

        path.moveTo(157.474f, 311.753f);
        path.cubicTo(157.474f, 311.753f, 85.5f, 229.5f, 35.5f, 229.5f);
        canvas->drawPath(path.detach(), p);
        path.moveTo(280, 250);
        path.quadTo(280, 250, 310, 280);
        canvas->drawPath(path.detach(), p);
        path.moveTo(180, 250);
        path.quadTo(180, 250, 210, 280);
        canvas->drawPath(path.detach(), p);
    }
};

//////////////////////////////////////////////////////////////////////////////

GMREGISTER(return new StrokesGM;)
GMREGISTER(return new Strokes2GM;)
GMREGISTER(return new Strokes3GM;)
GMREGISTER(return new Strokes4GM;)
GMREGISTER(return new Strokes5GM;)

GMREGISTER(return new ZeroLenStrokesGM;)
GMREGISTER(return new TeenyStrokesGM;)

#if 0
DEF_SIMPLE_GM(zerolinedash, canvas, 256, 256) {
    canvas->clear(SK_ColorWHITE);

    SkPaint paint;
    paint.setColor(SkColorSetARGB(255, 0, 0, 0));
    paint.setStrokeWidth(11);
    paint.setStrokeCap(SkPaint::kRound_Cap);
    paint.setStrokeJoin(SkPaint::kBevel_Join);

    SkScalar dash_pattern[] = {1, 5};
    paint.setPathEffect(SkDashPathEffect::Make(dash_pattern, 2, 0));

    canvas->drawLine(100, 100, 100, 100, paint);
}

#ifdef PDF_IS_FIXED_SO_THIS_DOESNT_BREAK_IT
DEF_SIMPLE_GM(longrect_dash, canvas, 250, 250) {
    canvas->clear(SK_ColorWHITE);

    SkPaint paint;
    paint.setColor(SkColorSetARGB(255, 0, 0, 0));
    paint.setStrokeWidth(5);
    paint.setStrokeCap(SkPaint::kRound_Cap);
    paint.setStrokeJoin(SkPaint::kBevel_Join);
    paint.setStyle(SkPaint::kStroke_Style);
    SkScalar dash_pattern[] = {1, 5};
    paint.setPathEffect(SkDashPathEffect::Make(dash_pattern, 2, 0));
    // try all combinations of stretching bounds
    for (auto left : {20.f, -100001.f}) {
        for (auto top : {20.f, -100001.f}) {
            for (auto right : {40.f, 100001.f}) {
                for (auto bottom : {40.f, 100001.f}) {
                    canvas->save();
                    canvas->clipRect({10, 10, 50, 50});
                    canvas->drawRect({left, top, right, bottom}, paint);
                    canvas->restore();
                    canvas->translate(60, 0);
                }
            }
            canvas->translate(-60 * 4, 60);
        }
    }
}
#endif
#endif

DEF_SIMPLE_GM(inner_join_geometry, 1000, 700, canvas)
{
    // These paths trigger cases where we must add inner join geometry.
    // skbug.com/11964
    const SkPoint pathPoints[] = {
        /*moveTo*/ /*lineTo*/ /*lineTo*/
        {119, 71},
        {129, 151},
        {230, 24},
        {200, 144},
        {129, 151},
        {230, 24},
        {192, 176},
        {224, 175},
        {281, 103},
        {233, 205},
        {224, 175},
        {281, 103},
        {121, 216},
        {234, 189},
        {195, 147},
        {141, 216},
        {254, 189},
        {238, 250},
        {159, 202},
        {269, 197},
        {289, 165},
        {159, 202},
        {269, 197},
        {287, 227},
    };

    Paint pathPaint;
    pathPaint->style(RenderPaintStyle::stroke);
    pathPaint->thickness(100);

#if 0
    Paint skeletonPaint;
    skeletonPaint->style(RenderPaintStyle::stroke);
    skeletonPaint->thickness(1);
    skeletonPaint->color(SK_ColorRED);
#endif

    canvas->translate(0, 50);
    for (size_t i = 0; i < SK_ARRAY_COUNT(pathPoints) / 3; i++)
    {
        // auto path = SkPath::Polygon(pathPoints + i * 3, 3, false);
        PathBuilder path;
        size_t j = i * 3;
        path.moveTo(pathPoints[j].x(), pathPoints[j].y());
        path.lineTo(pathPoints[j + 1].x(), pathPoints[j + 1].y());
        path.lineTo(pathPoints[j + 2].x(), pathPoints[j + 2].y());
        canvas->drawPath(path.detach(), pathPaint);

#if 0
        SkPath fillPath;
        pathPaint.getFillPath(path, &fillPath);
        canvas->drawPath(fillPath, skeletonPaint);
#endif

        canvas->translate(200, 0);
        if ((i + 1) % 4 == 0)
        {
            canvas->translate(-800, 200);
        }
    }
}

DEF_SIMPLE_GM(skbug12244, 150, 150, canvas)
{
    // Should look like a stroked triangle; these vertices are the results of the SkStroker
    // but we draw as a filled path in order to highlight that it's the GPU triangulating path
    // renderer that's the source of the problem, and not the stroking operation. The original
    // path was a simple:
    // m(0,0), l(100, 40), l(0, 80), l(0,0) with a stroke width of 15px
    Path path;
    path->moveTo(2.7854299545288085938, -6.9635753631591796875);
    path->lineTo(120.194366455078125, 40);
    path->lineTo(-7.5000004768371582031, 91.07775115966796875);
    path->lineTo(-7.5000004768371582031, -11.077748298645019531);
    path->lineTo(2.7854299545288085938, -6.9635753631591796875);
    path->moveTo(-2.7854299545288085938, 6.9635753631591796875);
    path->lineTo(0, 0);
    path->lineTo(7.5, 0);
    path->lineTo(7.5000004768371582031, 68.92224884033203125);
    path->lineTo(79.805633544921875, 40);
    path->lineTo(-2.7854299545288085938, 6.9635753631591796875);

    Paint p;
    p->color(SK_ColorGREEN);

    canvas->translate(20.f, 20.f);
    canvas->drawPath(path, p);
}

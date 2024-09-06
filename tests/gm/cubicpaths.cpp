/*
 * Copyright 2011 Google Inc.
 * Copyright 2022 Rive
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/shapes/paint/color.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/renderer.hpp"

using namespace rivegm;
using namespace rive;

// https://bug.skia.org/1316 shows that this cubic, when slightly clipped, creates big
// (incorrect) changes to its control points.
class ClippedCubicGM : public GM
{
public:
    ClippedCubicGM() : GM(600, 500, "clippedcubic") {}

private:
    void onDraw(Renderer* canvas) override
    {
        canvas->translate(25, 25);
        Path path;
        path->moveTo(0, 0);
        path->cubicTo(140, 150, 40, 10, 170, 150);

        Paint paint;
        AABB bounds = {0, 0, 170, 150}; // path.getBounds();

        for (float dy = -1; dy <= 1; dy += 1)
        {
            canvas->save();
            for (float dx = -1; dx <= 1; dx += 1)
            {
                canvas->save();
                canvas->clipPath(PathBuilder::Rect(bounds));
                canvas->translate(dx, dy);
                canvas->drawPath(path, paint);
                canvas->restore();

                canvas->translate(bounds.width(), 0);
            }
            canvas->restore();
            canvas->translate(0, bounds.height());
        }
    }
};

class ClippedCubic2GM : public GM
{
public:
    ClippedCubic2GM() : GM(725, 325, "clippedcubic2") {}

private:
    void onDraw(Renderer* canvas) override
    {
        canvas->translate(25, 25);
        canvas->save();
        canvas->translate(-2, 120);
        drawOne(canvas, fPath, {0, 0, 80, 150});
        canvas->translate(170, 0);
        drawOne(canvas, fPath, {0, 0, 80, 100});
        canvas->translate(170, 0);
        drawOne(canvas, fPath, {0, 0, 30, 150});
        canvas->translate(170, 0);
        drawOne(canvas, fPath, {0, 0, 10, 150});
        canvas->restore();
        canvas->save();
        canvas->translate(20, -2);
        drawOne(canvas, fFlipped, {0, 0, 150, 80});
        canvas->translate(170, 0);
        drawOne(canvas, fFlipped, {0, 0, 100, 80});
        canvas->translate(170, 0);
        drawOne(canvas, fFlipped, {0, 0, 150, 30});
        canvas->translate(170, 0);
        drawOne(canvas, fFlipped, {0, 0, 150, 10});
        canvas->restore();
    }

    void drawOne(Renderer* canvas, const Path& path, const AABB& clip)
    {
        Paint framePaint, fillPaint;
        framePaint->style(RenderPaintStyle::stroke);
        canvas->drawPath(PathBuilder::Rect(clip), framePaint);
        canvas->drawPath(path, framePaint);
        canvas->save();
        canvas->clipPath(PathBuilder::Rect(clip));
        canvas->drawPath(path, fillPaint);
        canvas->restore();
    }

    void onOnceBeforeDraw() override
    {
        fPath->moveTo(69.7030518991886f, 0);
        fPath->cubicTo(69.7030518991886f,
                       21.831149999999997f,
                       58.08369508178456f,
                       43.66448333333333f,
                       34.8449814469765f,
                       65.5f);
        fPath->cubicTo(11.608591683531916f,
                       87.33115f,
                       -0.010765133872116195f,
                       109.16448333333332f,
                       -0.013089005235602302f,
                       131);
        fPath->close();
        Mat2D matrix;
        matrix[0] = 0.0f;
        matrix[1] = 1.0f;
        matrix[2] = 1.0f;
        matrix[3] = 0.0f;
        fFlipped->addRenderPath(fPath, matrix);
    }

    Path fPath;
    Path fFlipped;
};

class CubicPathGM : public GM
{
public:
    CubicPathGM() : GM(825, 175, "cubicpath") {}

private:
    void drawPath(Path& path,
                  Renderer* canvas,
                  ColorInt color,
                  const AABB& clip,
                  StrokeCap cap,
                  StrokeJoin join,
                  RenderPaintStyle style,
                  FillRule fill,
                  float strokeWidth)
    {
        path->fillRule(fill);
        Paint paint;
        paint->cap(cap);
        paint->thickness(strokeWidth);
        paint->join(join);
        paint->color(color);
        paint->style(style);
        canvas->save();
        canvas->clipPath(PathBuilder::Rect(clip));
        canvas->drawPath(path, paint);
        canvas->restore();
    }

    void onDraw(Renderer* canvas) override
    {
        struct FillAndName
        {
            FillRule fFill;
            const char* fName;
        };
        constexpr FillAndName gFills[] = {
            {FillRule::nonZero, "Winding"},
            {FillRule::evenOdd, "Even / Odd"},
        };
        struct StyleAndName
        {
            RenderPaintStyle fStyle;
            const char* fName;
        };
        constexpr StyleAndName gStyles[] = {
            {RenderPaintStyle::fill, "Fill"},
            {RenderPaintStyle::stroke, "Stroke"},
        };
        struct CapAndName
        {
            StrokeCap fCap;
            StrokeJoin fJoin;
            const char* fName;
        };
        constexpr CapAndName gCaps[] = {{StrokeCap::butt, StrokeJoin::bevel, "Butt"},
                                        {StrokeCap::round, StrokeJoin::round, "Round"},
                                        {StrokeCap::square, StrokeJoin::bevel, "Square"}};
        struct PathAndName
        {
            Path fPath;
            const char* fName;
        };
        PathAndName path;
        path.fPath->moveTo(25 * 1.0f, 10 * 1.0f);
        path.fPath->cubicTo(40 * 1.0f, 20 * 1.0f, 60 * 1.0f, 20 * 1.0f, 75 * 1.0f, 10 * 1.0f);
        path.fName = "moveTo-cubic";

        AABB rect = {0, 0, 100 * 1.0f, 30 * 1.0f};
        canvas->save();
        canvas->translate(10 * 1.0f, 30 * 1.0f);
        canvas->save();
        for (size_t cap = 0; cap < std::size(gCaps); ++cap)
        {
            if (0 < cap)
            {
                canvas->translate((rect.width() + 40 * 1.0f) * std::size(gStyles), 0);
            }
            canvas->save();
            for (size_t fill = 0; fill < std::size(gFills); ++fill)
            {
                if (0 < fill)
                {
                    canvas->translate(0, rect.height() + 40 * 1.0f);
                }
                canvas->save();
                for (size_t style = 0; style < std::size(gStyles); ++style)
                {
                    if (0 < style)
                    {
                        canvas->translate(rect.width() + 40 * 1.0f, 0);
                    }

                    ColorInt color = 0xff007000;
                    this->drawPath(path.fPath,
                                   canvas,
                                   color,
                                   rect,
                                   gCaps[cap].fCap,
                                   gCaps[cap].fJoin,
                                   gStyles[style].fStyle,
                                   gFills[fill].fFill,
                                   1.0f * 10);

                    Paint rectPaint;
                    rectPaint->color(0xFF000000);
                    rectPaint->style(RenderPaintStyle::stroke);
                    rectPaint->thickness(-1);
                    canvas->drawPath(PathBuilder::Rect(rect), rectPaint);
                }
                canvas->restore();
            }
            canvas->restore();
        }
        canvas->restore();
        canvas->restore();
    }
};

class CubicClosePathGM : public GM
{
public:
    CubicClosePathGM() : GM(825, 175, "cubicclosepath") {}

private:
    void drawPath(Path& path,
                  Renderer* canvas,
                  ColorInt color,
                  const AABB& clip,
                  StrokeCap cap,
                  StrokeJoin join,
                  RenderPaintStyle style,
                  FillRule fill,
                  float strokeWidth)
    {
        path->fillRule(fill);
        Paint paint;
        paint->cap(cap);
        paint->thickness(strokeWidth);
        paint->join(join);
        paint->color(color);
        paint->style(style);
        canvas->save();
        canvas->clipPath(PathBuilder::Rect(clip));
        canvas->drawPath(path, paint);
        canvas->restore();
    }

    void onDraw(Renderer* canvas) override
    {
        struct FillAndName
        {
            FillRule fFill;
            const char* fName;
        };
        constexpr FillAndName gFills[] = {
            {FillRule::nonZero, "Winding"},
            {FillRule::evenOdd, "Even / Odd"},
        };
        struct StyleAndName
        {
            RenderPaintStyle fStyle;
            const char* fName;
        };
        constexpr StyleAndName gStyles[] = {
            {RenderPaintStyle::fill, "Fill"},
            {RenderPaintStyle::stroke, "Stroke"},
        };
        struct CapAndName
        {
            StrokeCap fCap;
            StrokeJoin fJoin;
            const char* fName;
        };
        constexpr CapAndName gCaps[] = {{StrokeCap::butt, StrokeJoin::bevel, "Butt"},
                                        {StrokeCap::round, StrokeJoin::round, "Round"},
                                        {StrokeCap::square, StrokeJoin::bevel, "Square"}};
        struct PathAndName
        {
            Path fPath;
            const char* fName;
        };
        PathAndName path;
        path.fPath->moveTo(25 * 1.0f, 10 * 1.0f);
        path.fPath->cubicTo(40 * 1.0f, 20 * 1.0f, 60 * 1.0f, 20 * 1.0f, 75 * 1.0f, 10 * 1.0f);
        path.fPath->close();
        path.fName = "moveTo-cubic-close";

        AABB rect = {0, 0, 100 * 1.0f, 30 * 1.0f};
        canvas->save();
        canvas->translate(10 * 1.0f, 30 * 1.0f);
        canvas->save();
        for (size_t cap = 0; cap < std::size(gCaps); ++cap)
        {
            if (0 < cap)
            {
                canvas->translate((rect.width() + 40 * 1.0f) * std::size(gStyles), 0);
            }
            canvas->save();
            for (size_t fill = 0; fill < std::size(gFills); ++fill)
            {
                if (0 < fill)
                {
                    canvas->translate(0, rect.height() + 40 * 1.0f);
                }
                canvas->save();
                for (size_t style = 0; style < std::size(gStyles); ++style)
                {
                    if (0 < style)
                    {
                        canvas->translate(rect.width() + 40 * 1.0f, 0);
                    }

                    ColorInt color = 0xff007000;
                    this->drawPath(path.fPath,
                                   canvas,
                                   color,
                                   rect,
                                   gCaps[cap].fCap,
                                   gCaps[cap].fJoin,
                                   gStyles[style].fStyle,
                                   gFills[fill].fFill,
                                   1.0f * 10);

                    Paint rectPaint;
                    rectPaint->color(0xFF000000);
                    rectPaint->style(RenderPaintStyle::stroke);
                    rectPaint->thickness(-1);
                    canvas->drawPath(PathBuilder::Rect(rect), rectPaint);
                }
                canvas->restore();
            }
            canvas->restore();
        }
        canvas->restore();
        canvas->restore();
    }
};

DEF_SIMPLE_GM(bug5099, 50, 50, canvas)
{
    Paint p;
    p->color(0xFFFF0000);
    p->style(RenderPaintStyle::stroke);
    p->thickness(10);

    Path path;
    path->moveTo(6, 27);
    path->cubicTo(31.5f, 1.5f, 3.5f, 4.5f, 29, 29);
    canvas->drawPath(path, p);
}

DEF_SIMPLE_GM(bug6083, 100, 50, canvas)
{
    Paint p;
    p->color(0xFFFF0000);
    p->style(RenderPaintStyle::stroke);
    p->thickness(15);
    canvas->translate(-500, -130);
    Path path;
    path->moveTo(500.988f, 155.200f);
    path->lineTo(526.109f, 155.200f);
    Vec2D p1 = {526.109f, 155.200f};
    Vec2D p2 = {525.968f, 212.968f};
    Vec2D p3 = {526.109f, 241.840f};
    path->cubicTo(p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
    canvas->drawPath(path, p);
    canvas->translate(50, 0);
    path = Path();
    p2 = Vec2D(525.968f, 213.172f);
    path->moveTo(500.988f, 155.200f);
    path->lineTo(526.109f, 155.200f);
    path->cubicTo(p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
    canvas->drawPath(path, p);
}

//////////////////////////////////////////////////////////////////////////////

GMREGISTER(return new CubicPathGM;)
GMREGISTER(return new CubicClosePathGM;)
GMREGISTER(return new ClippedCubicGM;)
GMREGISTER(return new ClippedCubic2GM;)

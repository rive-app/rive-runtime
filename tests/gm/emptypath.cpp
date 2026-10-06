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

using namespace rivegm;
using namespace rive;

static constexpr int kPtsCount = 3;
static constexpr Vec2D kPts[kPtsCount] = {
    {40, 40},
    {80, 40},
    {120, 40},
};

static Path make_path_move()
{
    PathBuilder builder;
    for (Vec2D p : kPts)
    {
        builder.moveTo(p.x, p.y);
    }
    return builder.detach();
}

static Path make_path_move_close()
{
    PathBuilder builder;
    for (Vec2D p : kPts)
    {
        builder.moveTo(p.x, p.y).close();
    }
    return builder.detach();
}

static Path make_path_move_line()
{
    PathBuilder builder;
    for (Vec2D p : kPts)
    {
        builder.moveTo(p.x, p.y).lineTo(p.x, p.y);
    }
    return builder.detach();
}

static Path make_path_move_mix()
{
    return PathBuilder()
        .moveTo(kPts[0].x, kPts[0].y)
        .moveTo(kPts[1].x, kPts[1].y)
        .close()
        .moveTo(kPts[2].x, kPts[2].y)
        .lineTo(kPts[2].x, kPts[2].y)
        .detach();
}

static Path make_path_move_addpath()
{
    Path inner = PathBuilder()
                     .moveTo(kPts[1].x, kPts[1].y)
                     .lineTo(kPts[1].x, kPts[1].y)
                     .detach();
    Path path;
    path->moveTo(kPts[0].x, kPts[0].y);
    path->addRenderPath(inner.get(), Mat2D());
    path->moveTo(kPts[2].x, kPts[2].y);
    return path;
}

static Path make_path_addpath_line()
{
    Path inner = PathBuilder()
                     .moveTo(kPts[0].x, kPts[0].y)
                     .lineTo(kPts[0].x, kPts[0].y)
                     .detach();
    Path path;
    path->addRenderPath(inner.get(), Mat2D());
    path->moveTo(kPts[1].x, kPts[1].y);
    path->moveTo(kPts[2].x, kPts[2].y);
    path->lineTo(kPts[2].x, kPts[2].y);
    return path;
}

class EmptyStrokeGM : public GM
{
public:
    struct Options
    {
        bool stroke = false, feather = false;
    };

    EmptyStrokeGM(Options options) : GM(180, 780), m_options(options) {}

private:
    void onDraw(Renderer* renderer) override
    {
        // Every subpath below is degenerate, so they should render as follows:
        // - open: a cap
        // - closed: a join
        // - isolated moveTo: caps the same as a zero-length lineTo.
        // Whether that covers the red dot underneath depends on the cap/join
        // style.
        static constexpr Path (*kProcs[])() = {
            make_path_move,         // isolated moves
            make_path_move_close,   // closed, so joins rather than caps
            make_path_move_line,    // zero-length lines
            make_path_move_mix,     // a move, a close and a line, left to right
            make_path_move_addpath, // addPath must not drop the caps around it
            make_path_addpath_line, // nor the ones that follow it
        };

        Paint paint;
        if (m_options.stroke)
        {
            paint->style(RenderPaintStyle::stroke);
            paint->thickness(21);
        }
        if (m_options.feather)
        {
            paint->feather(21);
        }

        Paint dotPaint;
        dotPaint->color(0xffff0000);
        dotPaint->style(RenderPaintStyle::fill);

        for (size_t j = 0; j < 3; ++j)
        {
            paint->cap(static_cast<StrokeCap>((3 - j) % 3));
            paint->join(static_cast<StrokeJoin>(j));
            for (auto proc : kProcs)
            {
                for (int i = 0; i < 3; ++i)
                {
                    renderer->drawPath(PathBuilder::Oval({kPts[i].x - 3.5f,
                                                          kPts[i].y - 3.5f,
                                                          kPts[i].x + 3.5f,
                                                          kPts[i].y + 3.5f}),
                                       dotPaint);
                }
                Path path = proc();
                path->fillRule(FillRule::clockwise);
                renderer->drawPath(path, paint);
                renderer->translate(0, 40);
            }
        }
    }

    Options m_options;
};
GMREGISTER(emptystroke,
           return new EmptyStrokeGM({.stroke = true, .feather = false});)
GMREGISTER(emptyfeather,
           return new EmptyStrokeGM({.stroke = false, .feather = true});)
GMREGISTER(emptystrokefeather,
           return new EmptyStrokeGM({.stroke = true, .feather = true});)

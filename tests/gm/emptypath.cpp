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

class EmptyStrokeGM : public GM
{
public:
    struct Options
    {
        bool stroke = false, feather = false;
    };

    EmptyStrokeGM(Options options, const char* name) :
        GM(180, 540, name), m_options(options)
    {}

private:
    void onDraw(Renderer* renderer) override
    {
        static constexpr Path (*kProcs[])() = {
            make_path_move,       // expect red red red
            make_path_move_close, // expect black black black
            make_path_move_line,  // expect black black black
            make_path_move_mix,   // expect red black black,
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
GMREGISTER(return new EmptyStrokeGM({.stroke = true, .feather = false},
                                    "emptystroke");)
GMREGISTER(return new EmptyStrokeGM({.stroke = false, .feather = true},
                                    "emptyfeather");)
GMREGISTER(return new EmptyStrokeGM({.stroke = true, .feather = true},
                                    "emptystrokefeather");)

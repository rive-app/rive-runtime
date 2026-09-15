/*
 * Copyright 2026 Rive
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/renderer.hpp"

#include "assets/nomoon.png.hpp"

using namespace rivegm;
using namespace rive;

// Interleaves additive draws (the "additiveness" paint value) with advanced
// blend modes so both features appear adjacent in draw order within one scene.
//
// In normal (synchronous shader) mode this exercises the batcher's rule that
// additive and advanced-blend draws never share a batch: the advanced draws
// force batch breaks around the additive sequences and everything renders
// correctly.
//
// Under --only_ubershaders every batch runs a fully-featured pipeline that
// compiles ENABLE_ADVANCED_BLEND, whose unmultiplied paint path does not
// decode the additive encodings. Comparing the right column (additive)
// against the left column (srcOver references with identical geometry) makes
// any ubershader divergence obvious.
//
// Layout: 2 columns x 4 rows of 300x300 cells on a gray background (gray so
// multiply visibly darkens and screen visibly brightens). The LEFT column is
// always pure srcOver and must stay bit-identical to the pre-additiveness
// master baseline forever; the RIGHT column is the additive variant.
//   Row 0: solid RGB venn circles interleaved with multiply + screen squares.
//          Left circles srcOver, right circles additive.
//   Row 1: gradient alpha-ramp overlays interleaved with a difference square,
//          plus a translucent solid on top of the advanced draw.
//   Row 2: drawImage interleaved with multiply + hardLight squares.
//   Row 3, left: additiveness must be ignored on advanced blend modes -- two
//          multiply squares with additiveness 0 and 1 must render identically
//          (and never change).
//   Row 3, right: the additiveness sweep (0, .25, .5, .75, 1) with a multiply
//          square drawn mid-sequence.
constexpr float CELL = 300.f;

static void draw_rect(Renderer* renderer,
                      const AABB& rect,
                      ColorInt color,
                      BlendMode blend,
                      float additiveness)
{
    Paint paint;
    paint->color(color);
    paint->blendMode(blend);
    paint->additiveness(additiveness);
    renderer->drawPath(PathBuilder::Rect(rect), paint);
}

static void draw_circle(Renderer* renderer,
                        float cx,
                        float cy,
                        ColorInt color,
                        float additiveness)
{
    Paint paint;
    paint->color(color);
    paint->additiveness(additiveness);
    renderer->drawPath(PathBuilder::Circle(cx, cy, 80.f), paint);
}

DEF_SIMPLE_GM_WITH_CLEAR_COLOR(additive_advanced_blend,
                               0xff404040,
                               600,
                               1200,
                               renderer)
{
    Factory* factory = TestingWindow::Get()->factory();
    constexpr float stops[2] = {0.f, 1.f};

    // Row 0: solid venn circles with advanced-blend squares drawn between
    // them. 'additiveness' selects srcOver (left cell) vs additive (right).
    auto vennCell = [&](float ox, float additiveness) {
        constexpr float oy = 0;
        draw_circle(renderer, ox + 150.f, oy + 110.f, 0xffff0000, additiveness);
        draw_rect(renderer,
                  {ox + 40.f, oy + 40.f, ox + 190.f, oy + 190.f},
                  0xff8080ff,
                  BlendMode::multiply,
                  0.f);
        draw_circle(renderer, ox + 110.f, oy + 185.f, 0xff00ff00, additiveness);
        draw_rect(renderer,
                  {ox + 110.f, oy + 110.f, ox + 260.f, oy + 260.f},
                  0xff404080,
                  BlendMode::screen,
                  0.f);
        draw_circle(renderer, ox + 190.f, oy + 185.f, 0xff0000ff, additiveness);
    };
    vennCell(0, 0.f);
    vennCell(CELL, 1.f);

    // Row 1: gradient overlays sandwiching a difference-blended gradient
    // square, with a translucent additive solid drawn last.
    auto gradientCell = [&](float ox, float additiveness) {
        constexpr float oy = CELL;
        AABB base = {ox + 40.f, oy + 40.f, ox + 260.f, oy + 260.f};
        ColorInt baseColors[2] = {0xff00c0ff, 0xffff8000};
        Paint basePaint;
        basePaint->shader(factory->makeLinearGradient(base.left(),
                                                      0,
                                                      base.right(),
                                                      0,
                                                      baseColors,
                                                      stops,
                                                      2));
        renderer->drawPath(PathBuilder::Rect(base), basePaint);

        AABB overlay = {ox + 70.f, oy + 70.f, ox + 170.f, oy + 230.f};
        ColorInt overlayColors[2] = {0xffff0000, 0x00ff0000};
        Paint overlayPaint;
        overlayPaint->shader(factory->makeLinearGradient(0,
                                                         overlay.top(),
                                                         0,
                                                         overlay.bottom(),
                                                         overlayColors,
                                                         stops,
                                                         2));
        overlayPaint->additiveness(additiveness);
        renderer->drawPath(PathBuilder::Rect(overlay), overlayPaint);

        AABB diff = {ox + 130.f, oy + 100.f, ox + 230.f, oy + 200.f};
        ColorInt diffColors[2] = {0xffffffff, 0xff000000};
        Paint diffPaint;
        diffPaint->shader(factory->makeLinearGradient(diff.left(),
                                                      0,
                                                      diff.right(),
                                                      0,
                                                      diffColors,
                                                      stops,
                                                      2));
        diffPaint->blendMode(BlendMode::difference);
        renderer->drawPath(PathBuilder::Rect(diff), diffPaint);

        draw_rect(renderer,
                  {ox + 100.f, oy + 160.f, ox + 260.f, oy + 240.f},
                  0x8000ff00,
                  BlendMode::srcOver,
                  additiveness);
    };
    gradientCell(0, 0.f);
    gradientCell(CELL, 1.f);

    // Row 2: drawImage interleaved with multiply + hardLight squares.
    auto img = LoadImage(assets::nomoon_png());
    if (img != nullptr)
    {
        auto imageCell = [&](float ox, float additiveness) {
            constexpr float oy = CELL * 2;
            auto drawImageRect =
                [&](const AABB& rect, float opacity, float imageAdditiveness) {
                    renderer->save();
                    renderer->translate(rect.left(), rect.top());
                    renderer->scale(rect.width() / img->width(),
                                    rect.height() / img->height());
                    renderer->drawImage(img.get(),
                                        ImageSampler::LinearClamp(),
                                        BlendMode::srcOver,
                                        opacity,
                                        imageAdditiveness);
                    renderer->restore();
                };
            drawImageRect({ox + 40.f, oy + 40.f, ox + 190.f, oy + 190.f},
                          1.f,
                          0.f);
            draw_rect(renderer,
                      {ox + 90.f, oy + 60.f, ox + 210.f, oy + 180.f},
                      0xffa0a0a0,
                      BlendMode::multiply,
                      0.f);
            drawImageRect({ox + 110.f, oy + 110.f, ox + 260.f, oy + 260.f},
                          .7f,
                          additiveness);
            draw_rect(renderer,
                      {ox + 60.f, oy + 170.f, ox + 180.f, oy + 270.f},
                      0xff808040,
                      BlendMode::hardLight,
                      0.f);
        };
        imageCell(0, 0.f);
        imageCell(CELL, 1.f);
    }

    // Row 3, left: these two multiply squares differ only in additiveness,
    // which advanced blend modes ignore -- they must render identically, and
    // this cell must never change.
    {
        constexpr float oy = CELL * 3;
        draw_rect(renderer,
                  {40.f, oy + 40.f, 140.f, oy + 260.f},
                  0xff80c0ff,
                  BlendMode::multiply,
                  0.f);
        draw_rect(renderer,
                  {160.f, oy + 40.f, 260.f, oy + 260.f},
                  0xff80c0ff,
                  BlendMode::multiply,
                  1.f);

        // Row 3, right: the additiveness sweep with a multiply square drawn
        // in the middle of the strip sequence, forcing a batch break within
        // the sweep.
        draw_rect(renderer,
                  {CELL + 40.f, oy + 40.f, CELL + 190.f, oy + 190.f},
                  0xff00ff00,
                  BlendMode::srcOver,
                  0.f);
        constexpr int strips = 5;
        AABB overlay = {CELL + 110.f, oy + 110.f, CELL + 260.f, oy + 260.f};
        for (int i = 0; i < strips; ++i)
        {
            if (i == 3)
            {
                draw_rect(
                    renderer,
                    {overlay.left(), oy + 180.f, overlay.right(), oy + 220.f},
                    0xff8080ff,
                    BlendMode::multiply,
                    0.f);
            }
            AABB strip = {overlay.left() + overlay.width() * i / strips,
                          overlay.top(),
                          overlay.left() + overlay.width() * (i + 1) / strips,
                          overlay.bottom()};
            Paint paint;
            paint->color(0xc0ff0000);
            paint->additiveness(float(i) / (strips - 1));
            renderer->drawPath(PathBuilder::Rect(strip), paint);
        }
    }
}

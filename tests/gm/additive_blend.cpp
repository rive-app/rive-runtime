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

#include <array>

using namespace rivegm;
using namespace rive;

// Exercises the additiveness paint value (a Rive extension), which
// interpolates the srcOver composite toward additive: 0 is normal srcOver,
// 1 is fully additive (result.rgb = src + dst, destination alpha preserved).
//
// Layout: 2 columns x 6 rows of 300x300 cells on a black background.
//
// Left column (srcOver references):
//   Row 0: opaque green square overlaid by a red square whose alpha ramps
//          from 1 at the top to 0 at the bottom.
//   Row 1: stroked variants of row 0.
//   Row 2: opaque gradient square overlaid by a gradient square whose alpha
//          ramps from 1 at the top to 0 at the bottom.
//   Row 3: image mesh overlaid by an image mesh whose opacity ramps from 1
//          at the top to 0 at the bottom (drawn as horizontal strips).
//   Row 4: drawImage (image paint on a path): base image at full opacity
//          under an overlapping image at 60% opacity.
//
// Right column (additive):
//   Row 0: filled RGB venn circles; overlaps accumulate to
//          yellow/cyan/magenta/white.
//   Row 1: stroked rings (additive applies to strokes independently).
//   Rows 2 & 3: same geometry as the left column, with the overlay drawn
//          additively, so left vs right isolates srcOver vs additive.
//
// Row 5: the additiveness slider (0 = srcOver, 1 = fully additive). An opaque
//        green square under five red strips with additiveness 0, .25, .5,
//        .75, 1 (srcOver to fully additive, left to right). Left column solid
//        color, right column gradient.
constexpr float CELL = 300.f;

// Draws one quad (2 triangles) of an image as a mesh.
static void draw_image_mesh_quad(Renderer* renderer,
                                 RenderImage* img,
                                 const AABB& dst,
                                 const AABB& uv,
                                 float additiveness,
                                 float opacity)
{
    Factory* factory = TestingWindow::Get()->factory();
    auto pts =
        factory->makeRenderBuffer(RenderBufferType::vertex,
                                  RenderBufferFlags::mappedOnceAtInitialization,
                                  4 * sizeof(Vec2D));
    memcpy(pts->map(),
           std::array<Vec2D, 4>{Vec2D{dst.left(), dst.top()},
                                Vec2D{dst.right(), dst.top()},
                                Vec2D{dst.left(), dst.bottom()},
                                Vec2D{dst.right(), dst.bottom()}}
               .data(),
           pts->sizeInBytes());
    pts->unmap();
    auto uvs =
        factory->makeRenderBuffer(RenderBufferType::vertex,
                                  RenderBufferFlags::mappedOnceAtInitialization,
                                  4 * sizeof(Vec2D));
    memcpy(uvs->map(),
           std::array<Vec2D, 4>{Vec2D{uv.left(), uv.top()},
                                Vec2D{uv.right(), uv.top()},
                                Vec2D{uv.left(), uv.bottom()},
                                Vec2D{uv.right(), uv.bottom()}}
               .data(),
           uvs->sizeInBytes());
    uvs->unmap();
    auto indices =
        factory->makeRenderBuffer(RenderBufferType::index,
                                  RenderBufferFlags::mappedOnceAtInitialization,
                                  6 * sizeof(uint16_t));
    memcpy(indices->map(),
           std::array<uint16_t, 6>{0, 1, 2, 1, 3, 2}.data(),
           indices->sizeInBytes());
    indices->unmap();
    renderer->drawImageMesh(img,
                            ImageSampler::LinearClamp(),
                            pts,
                            uvs,
                            indices,
                            4,
                            6,
                            BlendMode::srcOver,
                            opacity,
                            additiveness);
}

// Approximates a vertical opacity ramp (1 at the top, 0 at the bottom) by
// drawing the image as a stack of mesh strips with stepped opacity.
static void draw_image_mesh_alpha_ramp(Renderer* renderer,
                                       RenderImage* img,
                                       const AABB& dst,
                                       float additiveness)
{
    constexpr int strips = 256;
    for (int i = 0; i < strips; ++i)
    {
        float t0 = float(i) / strips;
        float t1 = float(i + 1) / strips;
        AABB stripDst = {dst.left(),
                         dst.top() + t0 * dst.height(),
                         dst.right(),
                         dst.top() + t1 * dst.height()};
        AABB stripUV = {0.f, t0, 1.f, t1};
        draw_image_mesh_quad(renderer,
                             img,
                             stripDst,
                             stripUV,
                             additiveness,
                             1.f - (t0 + t1) * .5f);
    }
}

DEF_SIMPLE_GM_WITH_CLEAR_COLOR(additive_blend, 0xff000000, 600, 1800, renderer)
{
    Factory* factory = TestingWindow::Get()->factory();
    constexpr float stops[2] = {0.f, 1.f};

    // Base square and overlapping overlay square within a cell at (ox, oy).
    auto baseRect = [](float ox, float oy) {
        return AABB{ox + 40.f, oy + 40.f, ox + 190.f, oy + 190.f};
    };
    auto overlayRect = [](float ox, float oy) {
        return AABB{ox + 110.f, oy + 110.f, ox + 260.f, oy + 260.f};
    };

    // Rows 0 & 1, left: opaque green square under a red square whose alpha
    // ramps 1 -> 0 top to bottom (via a gradient that only varies alpha).
    auto squaresCell = [&](float ox,
                           float oy,
                           RenderPaintStyle style,
                           float thickness,
                           BlendMode overlayBlend) {
        AABB base = baseRect(ox, oy);
        Paint basePaint;
        basePaint->style(style);
        if (style == RenderPaintStyle::stroke)
        {
            basePaint->thickness(thickness);
        }
        basePaint->color(0xff00ff00);
        renderer->drawPath(PathBuilder::Rect(base), basePaint);

        AABB overlay = overlayRect(ox, oy);
        ColorInt overlayColors[2] = {0xffff0000, 0x00ff0000};
        Paint overlayPaint;
        overlayPaint->style(style);
        if (style == RenderPaintStyle::stroke)
        {
            overlayPaint->thickness(thickness);
        }
        overlayPaint->shader(factory->makeLinearGradient(0,
                                                         overlay.top(),
                                                         0,
                                                         overlay.bottom(),
                                                         overlayColors,
                                                         stops,
                                                         2));
        overlayPaint->blendMode(overlayBlend);
        renderer->drawPath(PathBuilder::Rect(overlay), overlayPaint);
    };
    squaresCell(0, 0, RenderPaintStyle::fill, 0.f, BlendMode::srcOver);
    squaresCell(0, CELL, RenderPaintStyle::stroke, 20.f, BlendMode::srcOver);

    // Rows 0 & 1, right: classic RGB venn arrangement, all circles additive.
    // Two-circle overlaps accumulate to yellow/cyan/magenta, all three to
    // white.
    auto vennCell =
        [&](float ox, float oy, RenderPaintStyle style, float thickness) {
            struct Circle
            {
                float cx, cy;
                ColorInt color;
            };
            constexpr float radius = 80.f;
            const Circle circles[] = {
                {ox + 150.f, oy + 110.f, 0xffff0000}, // red, top
                {ox + 110.f, oy + 185.f, 0xff00ff00}, // green, bottom-left
                {ox + 190.f, oy + 185.f, 0xff0000ff}, // blue, bottom-right
            };
            for (const Circle& c : circles)
            {
                Paint paint;
                paint->style(style);
                if (style == RenderPaintStyle::stroke)
                {
                    paint->thickness(thickness);
                }
                paint->color(c.color);
                paint->additiveness(1.f);
                renderer->drawPath(PathBuilder::Circle(c.cx, c.cy, radius),
                                   paint);
            }
        };
    vennCell(CELL, 0, RenderPaintStyle::fill, 0.f);
    vennCell(CELL, CELL, RenderPaintStyle::stroke, 40.f);

    // Row 2: opaque horizontal gradient square under a vertical gradient
    // square that fades opaque red -> transparent yellow. Left srcOver,
    // right additive.
    auto gradientCell = [&](float ox, float overlayAdditiveness) {
        constexpr float oy = CELL * 2;
        AABB base = baseRect(ox, oy);
        ColorInt baseColors[2] = {0xff00ff00, 0xff0000ff};
        Paint basePaint;
        basePaint->shader(factory->makeLinearGradient(base.left(),
                                                      0,
                                                      base.right(),
                                                      0,
                                                      baseColors,
                                                      stops,
                                                      2));
        renderer->drawPath(PathBuilder::Rect(base), basePaint);

        AABB overlay = overlayRect(ox, oy);
        ColorInt overlayColors[2] = {0xffff0000, 0x00ffff00};
        Paint overlayPaint;
        overlayPaint->shader(factory->makeLinearGradient(0,
                                                         overlay.top(),
                                                         0,
                                                         overlay.bottom(),
                                                         overlayColors,
                                                         stops,
                                                         2));
        overlayPaint->additiveness(overlayAdditiveness);
        renderer->drawPath(PathBuilder::Rect(overlay), overlayPaint);
    };
    gradientCell(0, /*overlayAdditiveness =*/0.f);
    gradientCell(CELL, /*overlayAdditiveness =*/1.f);

    // Row 3: image mesh under an overlapping image mesh whose opacity ramps
    // 1 -> 0 top to bottom. Left srcOver, right additive.
    auto img = LoadImage(assets::nomoon_png());
    if (img != nullptr)
    {
        auto imageCell = [&](float ox, float overlayAdditiveness) {
            constexpr float oy = CELL * 3;
            draw_image_mesh_quad(renderer,
                                 img.get(),
                                 baseRect(ox, oy),
                                 {0.f, 0.f, 1.f, 1.f},
                                 /*additiveness =*/0.f,
                                 1.f);
            draw_image_mesh_alpha_ramp(renderer,
                                       img.get(),
                                       overlayRect(ox, oy),
                                       overlayAdditiveness);
        };
        imageCell(0, /*overlayAdditiveness =*/0.f);
        imageCell(CELL, /*overlayAdditiveness =*/1.f);

        // Row 4: drawImage (image paint on a path): base image at full
        // opacity under an overlapping image at 60% opacity. Left srcOver,
        // right additive.
        auto drawImageRect =
            [&](const AABB& rect, float additiveness, float opacity) {
                renderer->save();
                renderer->translate(rect.left(), rect.top());
                renderer->scale(rect.width() / img->width(),
                                rect.height() / img->height());
                renderer->drawImage(img.get(),
                                    ImageSampler::LinearClamp(),
                                    BlendMode::srcOver,
                                    opacity,
                                    additiveness);
                renderer->restore();
            };
        auto drawImageCell = [&](float ox, float overlayAdditiveness) {
            constexpr float oy = CELL * 4;
            drawImageRect(baseRect(ox, oy), /*additiveness =*/0.f, 1.f);
            drawImageRect(overlayRect(ox, oy), overlayAdditiveness, .6f);
        };
        drawImageCell(0, /*overlayAdditiveness =*/0.f);
        drawImageCell(CELL, /*overlayAdditiveness =*/1.f);
    }

    // Row 5: the additiveness slider. An opaque green square under five red
    // strips with additiveness 0, .25, .5, .75, 1 (srcOver to fully additive,
    // left to right). Left cell solid color, right cell gradient.
    auto additivenessSweepCell = [&](float ox, bool gradient) {
        constexpr float oy = CELL * 5;
        AABB base = baseRect(ox, oy);
        Paint basePaint;
        basePaint->color(0xff00ff00);
        renderer->drawPath(PathBuilder::Rect(base), basePaint);

        AABB overlay = overlayRect(ox, oy);
        constexpr int strips = 5;
        for (int i = 0; i < strips; ++i)
        {
            AABB strip = {overlay.left() + overlay.width() * i / strips,
                          overlay.top(),
                          overlay.left() + overlay.width() * (i + 1) / strips,
                          overlay.bottom()};
            Paint paint;
            if (gradient)
            {
                // Opaque red -> half-transparent red, top to bottom.
                ColorInt colors[2] = {0xffff0000, 0x80ff0000};
                paint->shader(factory->makeLinearGradient(0,
                                                          strip.top(),
                                                          0,
                                                          strip.bottom(),
                                                          colors,
                                                          stops,
                                                          2));
            }
            else
            {
                paint->color(0xc0ff0000);
            }
            paint->additiveness(float(i) / (strips - 1));
            renderer->drawPath(PathBuilder::Rect(strip), paint);
        }
    };
    additivenessSweepCell(0, /*gradient =*/false);
    additivenessSweepCell(CELL, /*gradient =*/true);
}

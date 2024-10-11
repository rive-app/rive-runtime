/*
 * Copyright 2013 Google Inc.
 * Copyright 2022 Rive
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/renderer.hpp"
#include "rive/math/math_types.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/math/mat2d.hpp"
#include "common/rand.hpp"

using namespace rivegm;
using namespace rive;

/**
 * Renders overlapping shapes with colorburn against a checkerboard.
 */
class DstReadShuffle : public GM
{
public:
    DstReadShuffle() : GM(530, 690, "dstreadshuffle") {}

protected:
    enum ShapeType
    {
        kCircle_ShapeType,
        kRoundRect_ShapeType,
        kRect_ShapeType,
        kConvexPath_ShapeType,
        kPolygonFan_ShapeType,
        kConcavePath_ShapeType,
        kNumShapeTypes
    };

    static void fan_polygon(PathBuilder& builder,
                            float a0,
                            float a1,
                            float x,
                            float y,
                            float rx,
                            float ry,
                            int depth)
    {
        if (depth <= 0)
        {
            return;
        }
        float ai = (a0 + a1) / 2;
        builder.moveTo(cosf(a0) * rx + x, sinf(a0) * ry + y);
        builder.lineTo(cosf(ai) * rx + x, sinf(ai) * ry + y);
        builder.lineTo(cosf(a1) * rx + x, sinf(a1) * ry + y);
        builder.close();
        fan_polygon(builder, a0, ai, x, y, rx, ry, depth - 1);
        fan_polygon(builder, ai, a1, x, y, rx, ry, depth - 1);
    }

    void drawShape(Renderer* renderer, const Paint& paint, ShapeType type)
    {
        const float l = 0, t = 0, r = 75, b = 85;
        const AABB kRect(l, t, r, b);
        PathBuilder builder;
        switch (type)
        {
            case kCircle_ShapeType:
                builder.addOval(kRect);
                break;
            case kRoundRect_ShapeType:
                builder.addRRect(kRect, 15.f, 15.f);
                break;
            case kRect_ShapeType:
                builder.addRect(kRect);
                break;
            case kConvexPath_ShapeType:
            {
                builder.moveTo(l, t);
                builder.quadTo(r, t, r, b);
                builder.quadTo(l, b, l, t);
                break;
            }
            case kPolygonFan_ShapeType:
            {
                float a0 = 0, a1 = 2 * math::PI / 3, a2 = 4 * math::PI / 3,
                      a3 = 2 * math::PI;
                float x = (l + r) / 2, y = (t + b) / 2;
                float rx = (r - l) / 2, ry = (b - t) / 2;
                builder.moveTo(cosf(a0) * rx + x, sinf(a0) * ry + y);
                builder.lineTo(cosf(a1) * rx + x, sinf(a1) * ry + y);
                builder.lineTo(cosf(a2) * rx + x, sinf(a2) * ry + y);
                builder.lineTo(cosf(a3) * rx + x, sinf(a3) * ry + y);
                builder.close();
                fan_polygon(builder, a0, a1, x, y, rx, ry, 5);
                fan_polygon(builder, a1, a2, x, y, rx, ry, 5);
                fan_polygon(builder, a2, a3, x, y, rx, ry, 5);
                break;
            }
            case kConcavePath_ShapeType:
            {
                Vec2D points[5] = {
                    {70.f * cosf(-math::PI / 2), 70.f * sinf(-math::PI / 2)}};
                Mat2D rot = Mat2D::fromRotation(2 * math::PI / 5);
                for (int i = 1; i < 5; ++i)
                {
                    rot.mapPoints(points + i, points + i - 1, 1);
                }
                builder.moveTo(points[0].x + 50, points[0].y + 70);
                for (int i = 0; i < 5; ++i)
                {
                    auto [x, y] = points[(2 * i) % 5];
                    builder.lineTo(x + 50, y + 70);
                }
                builder.fillRule(FillRule::evenOdd);
                break;
            }
            default:
                break;
        }
        renderer->drawPath(builder.detach(), paint);
    }

    static ColorInt GetColor(Rand* random)
    {
        return 0x80000000 | (random->u32() & 0xffffff);
    }

    // static void DrawHairlines(Renderer* renderer) {
    //     if (canvas->imageInfo().alphaType() == kOpaque_SkAlphaType) {
    //         draw_clear(renderer, kBackground);
    //     } else {
    //         canvas->clear(SK_ColorTRANSPARENT);
    //     }
    //     SkPaint hairPaint;
    //     hairPaint.setStyle(SkPaint::kStroke_Style);
    //     hairPaint.setStrokeWidth(0);
    //     hairPaint.setAntiAlias(true);
    //     static constexpr int kNumHairlines = 12;
    //     SkPoint pts[] = {{3.f, 7.f}, {29.f, 7.f}};
    //     Rand colorRandom;
    //     SkMatrix rot;
    //     rot.setRotate(360.f / kNumHairlines, 15.5f, 12.f);
    //     rot.postTranslate(3.f, 0);
    //     for (int i = 0; i < 12; ++i) {
    //         hairPaint.setColor(GetColor(&colorRandom));
    //         canvas->drawLine(pts[0], pts[1], hairPaint);
    //         rot.mapPoints(pts, 2);
    //     }
    // }

    void onDraw(Renderer* renderer) override
    {
        Paint p;
        p->color(0x80808080);
        draw_rect(renderer, {0, 0, 530, 690}, p);

        float y = 5;
        for (int i = 0; i < kNumShapeTypes; i++)
        {
            Rand colorRandom;
            ShapeType shapeType = static_cast<ShapeType>(i);
            float x = 5;
            for (int r = 0; r <= 15; r++)
            {
                Paint p;
                p->color(GetColor(&colorRandom));
                // In order to get some op combining on the GPU backend we do 2
                // src over for each xfer mode which requires a dst read
                constexpr static BlendMode blendModes[] = {
                    BlendMode::srcOver,
                    BlendMode::screen,
                    BlendMode::overlay,
                    BlendMode::darken,
                    BlendMode::lighten,
                    BlendMode::colorDodge,
                    BlendMode::colorBurn,
                    BlendMode::hardLight,
                    BlendMode::softLight,
                    BlendMode::exclusion,
                    BlendMode::hue,
                    BlendMode::saturation,
                    BlendMode::color,
                    BlendMode::luminosity,
                };
                p->blendMode(blendModes[r % std::size(blendModes)]);
                renderer->save();
                renderer->translate(x, y);
                this->drawShape(renderer, p, shapeType);
                renderer->restore();
                x += 15;
            }
            y += 110;
        }
        // // Draw hairlines to a surface and then draw that to the main canvas
        // with a zoom so that
        // // it is easier to see how they blend.
        // SkImageInfo info;
        // // Recording canvases don't have a color type.
        // if (SkColorType::kUnknown_SkColorType ==
        // canvas->imageInfo().colorType()) {
        //     info = SkImageInfo::MakeN32Premul(35, 35);
        // } else {
        //     info = SkImageInfo::Make(35, 35,
        //                              canvas->imageInfo().colorType(),
        //                              canvas->imageInfo().alphaType(),
        //                              canvas->imageInfo().refColorSpace());
        // }
        // auto surf = canvas->makeSurface(info);
        // if (!surf) {
        //     // Fall back to raster. Raster supports only one of the 8 bit
        //     per-channel RGBA or BGRA
        //     // formats. This fall back happens when running with
        //     --preAbandonGpuContext. if ((info.colorType() ==
        //     kRGBA_8888_SkColorType ||
        //          info.colorType() == kBGRA_8888_SkColorType) &&
        //         info.colorType() != kN32_SkColorType) {
        //         info = SkImageInfo::Make(35, 35,
        //                                  kN32_SkColorType,
        //                                  canvas->imageInfo().alphaType(),
        //                                  canvas->imageInfo().refColorSpace());
        //     }
        //     surf = SkSurface::MakeRaster(info);
        //     SkASSERT(surf);
        // }
        // canvas->scale(5.f, 5.f);
        // canvas->translate(67.f, 10.f);
        // DrawHairlines(surf->getCanvas());
        // canvas->drawImage(surf->makeImageSnapshot(), 0.f, 0.f);
    }

private:
    static constexpr ColorInt kBackground = 0xFFCCCCCC;
    using INHERITED = GM;
};

//////////////////////////////////////////////////////////////////////////////

GMREGISTER(return new DstReadShuffle;)

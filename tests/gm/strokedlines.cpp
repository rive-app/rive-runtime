/*
 * Copyright 2016 Google Inc.
 * Copyright 2022 Rive
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/renderer.hpp"
#include "skia/include/core/SkMatrix.h"

using namespace rivegm;
using namespace rive;

constexpr int kNumColumns = 3;
constexpr int kNumPaints = 2;
constexpr int kNumRows = 5;
constexpr int kRadius = 40; // radius of the snowflake
constexpr int kPad = 5;     // padding on both sides of the snowflake
constexpr int kNumSpokes = 6;
constexpr float kStrokeWidth = 5.0f;

static void draw_fins(Renderer* renderer, const SkPoint& offset, float angle, RenderPaint* paint)
{
    float cos, sin;

    // first fin
    sin = SkScalarSin(angle + (SK_ScalarPI / 4));
    cos = SkScalarCos(angle + (SK_ScalarPI / 4));
    sin *= kRadius / 2.0f;
    cos *= kRadius / 2.0f;

    Path p;
    p->moveTo(offset.fX, offset.fY);
    p->lineTo(offset.fX + cos, offset.fY + sin);
    renderer->drawPath(p, paint);

    // second fin
    sin = SkScalarSin(angle - (SK_ScalarPI / 4));
    cos = SkScalarCos(angle - (SK_ScalarPI / 4));
    sin *= kRadius / 2.0f;
    cos *= kRadius / 2.0f;

    p = Path();
    p->moveTo(offset.fX, offset.fY);
    p->lineTo(offset.fX + cos, offset.fY + sin);
    renderer->drawPath(p, paint);
}

// draw a snowflake centered at the origin
static void draw_snowflake(Renderer* renderer, RenderPaint* paint)
{
    renderer->clipPath(
        PathBuilder::Rect({-kRadius - kPad, -kRadius - kPad, kRadius + kPad, kRadius + kPad}));

    float sin, cos, angle = 0.0f;
    for (int i = 0; i < kNumSpokes / 2; ++i, angle += SK_ScalarPI / (int)(kNumSpokes / 2))
    {
        sin = SkScalarSin(angle);
        cos = SkScalarCos(angle);
        sin *= kRadius;
        cos *= kRadius;

        // main spoke
        Path p;
        p->moveTo(-cos, -sin);
        p->lineTo(cos, sin);
        renderer->drawPath(p, paint);

        // fins on positive side
        const SkPoint posOffset = SkPoint::Make(0.5f * cos, 0.5f * sin);
        draw_fins(renderer, posOffset, angle, paint);

        // fins on negative side
        const SkPoint negOffset = SkPoint::Make(-0.5f * cos, -0.5f * sin);
        draw_fins(renderer, negOffset, angle + SK_ScalarPI, paint);
    }
}

static void draw_row(Renderer* renderer, RenderPaint* paint, const SkMatrix& localMatrix)
{
    renderer->translate(kRadius + kPad, 0.0f);

    for (auto cap : {StrokeCap::butt, StrokeCap::round, StrokeCap::square})
    {
        paint->thickness(kStrokeWidth);
        paint->style(RenderPaintStyle::stroke);
        paint->cap(cap);

        renderer->save();
        renderer->transform(mat2d_fromSkMatrix(localMatrix));
        draw_snowflake(renderer, paint);
        renderer->restore();

        renderer->translate(2 * (kRadius + kPad), 0.0f);
    }
}

namespace skiagm
{

// This GM exercises the special case of a stroked lines.
// Various shaders are applied to ensure the coordinate spaces work out right.
class StrokedLinesGM : public GM
{
public:
    StrokedLinesGM() :
        GM(kNumColumns * (2 * kRadius + 2 * kPad),
           kNumRows * (2 * kRadius + 2 * kPad),
           "strokedlines")
    {}

protected:
    void onOnceBeforeDraw() override
    {
        // paints
        {
            // basic white
            fPaints[0]->color(0xffffffff);
        }
        {
            // gradient
            ColorInt colors[] = {0xffff0000, 0xff00ff00};
            SkPoint pts[] = {{-kRadius - kPad, -kRadius - kPad}, {kRadius + kPad, kRadius + kPad}};
            float stops[] = {0, 1};

            auto sh = TestingWindow::Get()->factory()->makeLinearGradient(pts[0].x(),
                                                                          pts[0].y(),
                                                                          pts[1].x(),
                                                                          pts[1].y(),
                                                                          colors,
                                                                          stops,
                                                                          2);
            fPaints[1]->shader(sh);
        }
        // {
        //     // dashing
        //     float intervals[] = {kStrokeWidth, kStrokeWidth};
        //     int intervalCount = (int)SK_ARRAY_COUNT(intervals);
        //     SkPaint p;
        //     p.setColor(SK_ColorWHITE);
        //     p.setPathEffect(SkDashPathEffect::Make(intervals, intervalCount, kStrokeWidth));
        //
        //     fPaints.push_back(p);
        // }
        // {
        //     // Bitmap shader
        //     SkBitmap bm;
        //     bm.allocN32Pixels(2, 2);
        //     *bm.getAddr32(0, 0) = *bm.getAddr32(1, 1) = 0xFFFFFFFF;
        //     *bm.getAddr32(1, 0) = *bm.getAddr32(0, 1) = 0x0;
        //
        //     SkMatrix m;
        //     m.setRotate(12.0f);
        //     m.preScale(3.0f, 3.0f);
        //
        //     SkPaint p;
        //     p.setShader(
        //         bm.makeShader(SkTileMode::kRepeat, SkTileMode::kRepeat, SkSamplingOptions(),
        //         m));
        //     fPaints.push_back(p);
        // }
        // {
        //     // blur
        //     SkPaint p;
        //     p.setColor(SK_ColorWHITE);
        //     p.setMaskFilter(SkMaskFilter::MakeBlur(kOuter_SkBlurStyle, 3.0f));
        //     fPaints.push_back(p);
        // }

        // matrices
        {
            // rotation
            SkMatrix m;
            m.setRotate(12.0f);

            fMatrices.push_back(m);
        }
        {
            // skew
            SkMatrix m;
            m.setSkew(0.3f, 0.5f);

            fMatrices.push_back(m);
        }
        {
            // perspective
            SkMatrix m;
            m.reset();
            m.setPerspX(-SK_Scalar1 / 300);
            m.setPerspY(SK_Scalar1 / 300);

            fMatrices.push_back(m);
        }

        SkASSERT(kNumRows == kNumPaints + fMatrices.size());
    }

    ColorInt clearColor() const override { return 0xFF1A65D7; }

    void onDraw(rive::Renderer* renderer) override
    {
        renderer->translate(0, kRadius + kPad);

        for (int i = 0; i < kNumPaints; ++i)
        {
            renderer->save();
            draw_row(renderer, fPaints[i], SkMatrix::I());
            renderer->restore();

            renderer->translate(0, 2 * (kRadius + kPad));
        }

        for (size_t i = 0; i < fMatrices.size(); ++i)
        {
            renderer->save();
            draw_row(renderer, fPaints[0], fMatrices[i]);
            renderer->restore();

            renderer->translate(0, 2 * (kRadius + kPad));
        }
    }

private:
    Paint fPaints[kNumPaints];
    std::vector<SkMatrix> fMatrices;

    using INHERITED = GM;
};

//////////////////////////////////////////////////////////////////////////////

GMREGISTER(return new StrokedLinesGM;)
} // namespace skiagm

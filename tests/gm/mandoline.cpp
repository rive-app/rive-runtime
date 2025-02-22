/*
 * Copyright 2018 Google Inc.
 * Copyright 2022 Rive
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/renderer.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/math/bezier_utils.hpp"
#include "rive/math/math_types.hpp"
#include "common/rand.hpp"

using namespace rivegm;
using namespace rive;
using namespace rive::math;

// Atomic mode uses 6:11 fixed point and clockwiseAtomic uses 7:8, so the
// winding number breaks if a shape has more than +/-32 [ == +/-2^(6-1)] levels
// of self overlap at any point.
constexpr static float SLICE_LENGTH = 2;

// Slices paths into sliver-size contours shaped like ice cream cones.
class MandolineSlicer
{
public:
    MandolineSlicer(Vec2D anchorPt, Vec2D p0) { this->reset(anchorPt, p0); }

    void reset(Vec2D anchorPt, Vec2D p0)
    {
        fPath = Path();
        fPath->fillRule(FillRule::evenOdd);
        fAnchorPt = anchorPt;
        fLastPt = p0;
    }

    void sliceLine(Vec2D pt)
    {
        if ((pt - fLastPt).length() < SLICE_LENGTH)
        {
            fPath->moveTo(fAnchorPt.x, fAnchorPt.y);
            fPath->lineTo(fLastPt.x, fLastPt.y);
            fPath->lineTo(pt.x, pt.y);
            fPath->close();
            fLastPt = pt;
            return;
        }
        float T = .5f;
        if (0 == T)
        {
            return;
        }
        Vec2D midpt = fLastPt * (1 - T) + pt * T;
        this->sliceLine(midpt);
        this->sliceLine(pt);
    }

    void sliceQuadratic(Vec2D p1, Vec2D p2)
    {
        if ((p2 - fLastPt).length() < SLICE_LENGTH)
        {
            fPath->moveTo(fAnchorPt.x, fAnchorPt.y);
            fPath->lineTo(fLastPt.x, fLastPt.y);
            fPath->cubicTo(fLastPt.x + (p1.x - fLastPt.x) * (2 / 3.f),
                           fLastPt.y + (p1.y - fLastPt.y) * (2 / 3.f),
                           p2.x + (p1.x - p2.x) * (2 / 3.f),
                           p2.y + (p1.y - p2.y) * (2 / 3.f),
                           p2.x,
                           p2.y);
            fPath->close();
            fLastPt = p2;
            return;
        }
        Vec2D P[4] = {fLastPt,
                      Vec2D::lerp(fLastPt, p1, 2 / 3.f),
                      Vec2D::lerp(p2, p1, 2 / 3.f),
                      p2},
              PP[7];
        math::chop_cubic_at(P, PP, .5f);

        this->sliceCubic(PP[1], PP[2], PP[3]);
        this->sliceCubic(PP[4], PP[5], PP[6]);
    }

    void sliceCubic(Vec2D p1, Vec2D p2, Vec2D p3)
    {
        if ((p3 - fLastPt).length() < SLICE_LENGTH)
        {
            fPath->moveTo(fAnchorPt.x, fAnchorPt.y);
            fPath->lineTo(fLastPt.x, fLastPt.y);
            fPath->cubicTo(p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
            fPath->close();
            fLastPt = p3;
            return;
        }
        Vec2D P[4] = {fLastPt, p1, p2, p3}, PP[7];
        math::chop_cubic_at(P, PP, .5f);
        this->sliceCubic(PP[1], PP[2], PP[3]);
        this->sliceCubic(PP[4], PP[5], PP[6]);
    }

    rive::RenderPath* path() const { return fPath; }

private:
    Rand fRand;
    Path fPath;
    Vec2D fAnchorPt;
    Vec2D fLastPt;
};

class MandolineGM : public GM
{
public:
    MandolineGM() : GM(560, 475) {}

protected:
    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(Renderer* renderer) override
    {
        Paint paint;
        paint->color(0xc0c0c0c0);

        MandolineSlicer mandoline({0, -500}, {41, 43});
        mandoline.sliceCubic({5, 277}, {381, -74}, {243, 162});
        mandoline.sliceLine({41, 43});
        renderer->drawPath(mandoline.path(), paint);

        mandoline.reset({700, 700}, {357.049988f, 446.049988f});
        mandoline.sliceCubic({472.750000f, -71.950012f},
                             {639.750000f, 531.950012f},
                             {309.049988f, 347.950012f});
        mandoline.sliceLine({309.049988f, 419});
        mandoline.sliceLine({357.049988f, 446.049988f});
        renderer->drawPath(mandoline.path(), paint);

        renderer->save();
        renderer->translate(421, 105);
        renderer->scale(100, 81);
        mandoline.reset(
            {0, 500},
            {-cosf(degreesToRadians(-60)), sinf(degreesToRadians(-60))});
        mandoline.sliceQuadratic(
            {-2, 0},
            {-cosf(degreesToRadians(60)), sinf(degreesToRadians(60))});
        mandoline.sliceQuadratic(
            {-cosf(degreesToRadians(120)) * 2, sinf(degreesToRadians(120)) * 2},
            {1, 0});
        mandoline.sliceLine({0, 0});
        mandoline.sliceLine(
            {-cosf(degreesToRadians(-60)), sinf(degreesToRadians(-60))});
        renderer->drawPath(mandoline.path(), paint);
        renderer->restore();

        renderer->save();
        renderer->translate(150, 300);
        renderer->scale(75, 75);
        mandoline.reset({0, 0}, {1, 0});
        constexpr int nquads = 5;
        for (int i = 0; i < nquads; ++i)
        {
            float theta1 = 2 * PI / nquads * (i + .5f);
            float theta2 = 2 * PI / nquads * (i + 1);
            mandoline.sliceQuadratic({cosf(theta1) * 2, sinf(theta1) * 2},
                                     {cosf(theta2), sinf(theta2)});
        }
        renderer->drawPath(mandoline.path(), paint);
        renderer->restore();
    }
};

GMREGISTER_SLOW(mandoline, return new MandolineGM;)

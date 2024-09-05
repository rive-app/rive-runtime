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
#include "rive/renderer/render_context.hpp"
#include "include/core/SkPoint.h"
#include "include/utils/SkRandom.h"

using namespace rivegm;
using namespace rive;

extern void SkChopQuadAt(const SkPoint src[3], SkPoint dst[5], SkScalar t);
extern void SkChopCubicAt(const SkPoint src[4], SkPoint dst[7], SkScalar t);

// Slices paths into sliver-size contours shaped like ice cream cones.
class MandolineSlicer
{
public:
    MandolineSlicer(SkPoint anchorPt) { this->reset(anchorPt); }

    void reset(SkPoint anchorPt)
    {
        fPath = Path();
        fPath->fillRule(FillRule::evenOdd);
        fLastPt = fAnchorPt = anchorPt;
    }

    void sliceLine(SkPoint pt, int numSubdivisions)
    {
        if (numSubdivisions <= 0)
        {
            fPath->moveTo(fAnchorPt.x(), fAnchorPt.y());
            fPath->lineTo(fLastPt.x(), fLastPt.y());
            fPath->lineTo(pt.x(), pt.y());
            fPath->close();
            fLastPt = pt;
            return;
        }
        float T = this->chooseChopT(numSubdivisions);
        if (0 == T)
        {
            return;
        }
        SkPoint midpt = fLastPt * (1 - T) + pt * T;
        this->sliceLine(midpt, numSubdivisions - 1);
        this->sliceLine(pt, numSubdivisions - 1);
    }

    void sliceQuadratic(SkPoint p1, SkPoint p2, int numSubdivisions)
    {
        if (numSubdivisions <= 0)
        {
            fPath->moveTo(fAnchorPt.x(), fAnchorPt.y());
            fPath->lineTo(fLastPt.x(), fLastPt.y());
            fPath->cubicTo(fLastPt.x() + (p1.x() - fLastPt.x()) * (2 / 3.f),
                           fLastPt.y() + (p1.y() - fLastPt.y()) * (2 / 3.f),
                           p2.x() + (p1.x() - p2.x()) * (2 / 3.f),
                           p2.y() + (p1.y() - p2.y()) * (2 / 3.f),
                           p2.x(),
                           p2.y());
            fPath->close();
            fLastPt = p2;
            return;
        }
        float T = this->chooseChopT(numSubdivisions);
        if (0 == T)
        {
            return;
        }
        SkPoint P[3] = {fLastPt, p1, p2}, PP[5];
        SkChopQuadAt(P, PP, T);
        this->sliceQuadratic(PP[1], PP[2], numSubdivisions - 1);
        this->sliceQuadratic(PP[3], PP[4], numSubdivisions - 1);
    }

    void sliceCubic(SkPoint p1, SkPoint p2, SkPoint p3, int numSubdivisions)
    {
        if (numSubdivisions <= 0)
        {
            fPath->moveTo(fAnchorPt.x(), fAnchorPt.y());
            fPath->lineTo(fLastPt.x(), fLastPt.y());
            fPath->cubicTo(p1.x(), p1.y(), p2.x(), p2.y(), p3.x(), p3.y());
            fPath->close();
            fLastPt = p3;
            return;
        }
        float T = this->chooseChopT(numSubdivisions);
        if (0 == T)
        {
            return;
        }
        SkPoint P[4] = {fLastPt, p1, p2, p3}, PP[7];
        SkChopCubicAt(P, PP, T);
        this->sliceCubic(PP[1], PP[2], PP[3], numSubdivisions - 1);
        this->sliceCubic(PP[4], PP[5], PP[6], numSubdivisions - 1);
    }

    rive::RenderPath* path() const { return fPath; }

private:
    float chooseChopT(int numSubdivisions)
    {
        SkASSERT(numSubdivisions > 0);
        if (numSubdivisions > 1)
        {
            return .5f;
        }
        float T = (0 == fRand.nextU() % 10) ? 0 : scalbnf(1, -(int)fRand.nextRangeU(10, 149));
        SkASSERT(T >= 0 && T < 1);
        return T;
    }

    SkRandom fRand;
    Path fPath;
    SkPoint fAnchorPt;
    SkPoint fLastPt;
};

class MandolineGM : public GM
{
public:
    MandolineGM() : GM(560, 475, "mandoline") {}

protected:
    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(Renderer* renderer) override
    {
        int subdivisions = 12;
        if (gpu::RenderContext* renderContext = TestingWindow::Get()->renderContext())
        {
            if (renderContext->frameInterlockMode() == gpu::InterlockMode::atomics)
            {
                // Atomic mode uses 7:9 fixed point, so the winding number breaks if a shape has
                // more than 128 levels of self overlap at any point.
                subdivisions = 4;
            }
        }

        Paint paint;
        paint->color(0xc0c0c0c0);

        MandolineSlicer mandoline({41, 43});
        mandoline.sliceCubic({5, 277}, {381, -74}, {243, 162}, subdivisions);
        mandoline.sliceLine({41, 43}, subdivisions);
        renderer->drawPath(mandoline.path(), paint);

        mandoline.reset({357.049988f, 446.049988f});
        mandoline.sliceCubic({472.750000f, -71.950012f},
                             {639.750000f, 531.950012f},
                             {309.049988f, 347.950012f},
                             subdivisions);
        mandoline.sliceLine({309.049988f, 419}, subdivisions);
        mandoline.sliceLine({357.049988f, 446.049988f}, subdivisions);
        renderer->drawPath(mandoline.path(), paint);

        renderer->save();
        renderer->translate(421, 105);
        renderer->scale(100, 81);
        mandoline.reset({-cosf(SkDegreesToRadians(-60)), sinf(SkDegreesToRadians(-60))});
        mandoline.sliceQuadratic({-2, 0},
                                 {-cosf(SkDegreesToRadians(60)), sinf(SkDegreesToRadians(60))},
                                 subdivisions);
        mandoline.sliceQuadratic(
            {-cosf(SkDegreesToRadians(120)) * 2, sinf(SkDegreesToRadians(120)) * 2},
            {1, 0},
            subdivisions);
        mandoline.sliceLine({0, 0}, subdivisions);
        mandoline.sliceLine({-cosf(SkDegreesToRadians(-60)), sinf(SkDegreesToRadians(-60))},
                            subdivisions);
        renderer->drawPath(mandoline.path(), paint);
        renderer->restore();

        renderer->save();
        renderer->translate(150, 300);
        renderer->scale(75, 75);
        mandoline.reset({1, 0});
        constexpr int nquads = 5;
        for (int i = 0; i < nquads; ++i)
        {
            float theta1 = 2 * SK_ScalarPI / nquads * (i + .5f);
            float theta2 = 2 * SK_ScalarPI / nquads * (i + 1);
            mandoline.sliceQuadratic({cosf(theta1) * 2, sinf(theta1) * 2},
                                     {cosf(theta2), sinf(theta2)},
                                     subdivisions);
        }
        renderer->drawPath(mandoline.path(), paint);
        renderer->restore();
    }
};

GMREGISTER_SLOW(return new MandolineGM;)

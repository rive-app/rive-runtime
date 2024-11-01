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
#include "rive/math/vec2d.hpp"
#include "rive/math/math_types.hpp"
#include "common/rand.hpp"
#include "path_utils.hpp"

using namespace rivegm;
using namespace rive;
using namespace rive::math;
using namespace rive::pathutils;

static Vec2D SkFindBisector(Vec2D a, Vec2D b)
{
    std::array<Vec2D, 2> v;
    if (Vec2D::dot(a, b) >= 0)
    {
        // a,b are within +/-90 degrees apart.
        v = {a, b};
    }
    else if (Vec2D::cross(a, b) >= 0)
    {
        // a,b are >90 degrees apart. Find the bisector of their interior
        // normals instead. (Above 90 degrees, the original vectors start
        // cancelling each other out which eventually becomes unstable.)
        v[0] = {-a.y, +a.x};
        v[1] = {+b.y, -b.x};
    }
    else
    {
        // a,b are <-90 degrees apart. Find the bisector of their interior
        // normals instead. (Below -90 degrees, the original vectors start
        // cancelling each other out which eventually becomes unstable.)
        v[0] = {+a.y, -a.x};
        v[1] = {-b.y, +b.x};
    }
    // Return "normalize(v[0]) + normalize(v[1])".
    float2 x0_x1{v[0].x, v[1].x};
    float2 y0_y1{v[0].y, v[1].y};
    auto invLengths = 1.0f / simd::sqrt(x0_x1 * x0_x1 + y0_y1 * y0_y1);
    x0_x1 *= invLengths;
    y0_y1 *= invLengths;
    return Vec2D{x0_x1[0] + x0_x1[1], y0_y1[0] + y0_y1[1]};
}

static float SkFindQuadMidTangent(const Vec2D src[3])
{
    // Tangents point in the direction of increasing T, so tan0 and -tan1 both
    // point toward the midtangent. The bisector of tan0 and -tan1 is orthogonal
    // to the midtangent:
    //
    //     n dot midtangent = 0
    //
    Vec2D tan0 = src[1] - src[0];
    Vec2D tan1 = src[2] - src[1];
    Vec2D bisector = SkFindBisector(tan0, -tan1);

    // The midtangent can be found where (F' dot bisector) = 0:
    //
    //   0 = (F'(T) dot bisector) = |2*T 1| * |p0 - 2*p1 + p2| * |bisector.x|
    //                                        |-2*p0 + 2*p1  |   |bisector.y|
    //
    //                     = |2*T 1| * |tan1 - tan0| * |nx|
    //                                 |2*tan0     |   |ny|
    //
    //                     = 2*T * ((tan1 - tan0) dot bisector) + (2*tan0 dot
    //                     bisector)
    //
    //   T = (tan0 dot bisector) / ((tan0 - tan1) dot bisector)
    float T = Vec2D::dot(tan0, bisector) / Vec2D::dot(tan0 - tan1, bisector);
    if (!(T > 0 && T < 1))
    {           // Use "!(positive_logic)" so T=nan will take this branch.
        T = .5; // The quadratic was a line or near-line. Just chop at .5.
    }

    return T;
}

// Finds the root nearest 0.5. Returns 0.5 if the roots are undefined or outside
// 0..1.
static float solve_quadratic_equation_for_midtangent(float a,
                                                     float b,
                                                     float c,
                                                     float discr)
{
    // Quadratic formula from Numerical Recipes in C:
    float q = -.5f * (b + copysignf(sqrtf(discr), b));
    // The roots are q/a and c/q. Pick the midtangent closer to T=.5.
    float _5qa = -.5f * q * a;
    float T = fabsf(q * q + _5qa) < fabsf(a * c + _5qa) ? q / a : c / q;
    if (!(T > 0 && T < 1))
    { // Use "!(positive_logic)" so T=NaN will take this branch.
        // Either the curve is a flat line with no rotation or FP precision
        // failed us. Chop at .5.
        T = .5;
    }
    return T;
}

static float4 fma(float4 f, float4 m, float4 a) { return f * m + a; }

float SkFindCubicMidTangent(const Vec2D src[4])
{
    // Tangents point in the direction of increasing T, so tan0 and -tan1 both
    // point toward the midtangent. The bisector of tan0 and -tan1 is orthogonal
    // to the midtangent:
    //
    //     bisector dot midtangent == 0
    //
    Vec2D tan0 = (src[0] == src[1]) ? src[2] - src[0] : src[1] - src[0];
    Vec2D tan1 = (src[2] == src[3]) ? src[3] - src[1] : src[3] - src[2];
    Vec2D bisector = SkFindBisector(tan0, -tan1);

    // Find the T value at the midtangent. This is a simple quadratic equation:
    //
    //     midtangent dot bisector == 0, or using a tangent matrix C' in power
    //     basis form:
    //
    //                   |C'x  C'y|
    //     |T^2  T  1| * |.    .  | * |bisector.x| == 0
    //                   |.    .  |   |bisector.y|
    //
    // The coeffs for the quadratic equation we need to solve are therefore:  C'
    // * bisector
    static const float4 kM[3] = {float4{-1, 2, -1, 0},
                                 float4{3, -4, 1, 0},
                                 float4{-3, 2, 0, 0}};
    auto C_x = fma(
        kM[0],
        src[0].x,
        fma(kM[1], src[1].x, fma(kM[2], src[2].x, float4{src[3].x, 0, 0, 0})));
    auto C_y = fma(
        kM[0],
        src[0].y,
        fma(kM[1], src[1].y, fma(kM[2], src[2].y, float4{src[3].y, 0, 0, 0})));
    auto coeffs = C_x * bisector.x + C_y * bisector.y;

    // Now solve the quadratic for T.
    float T = 0;
    float a = coeffs[0], b = coeffs[1], c = coeffs[2];
    float discr = b * b - 4 * a * c;
    if (discr > 0)
    { // This will only be false if the curve is a line.
        return solve_quadratic_equation_for_midtangent(a, b, c, discr);
    }
    else
    {
        // This is a 0- or 360-degree flat line. It doesn't have single points
        // of midtangent. (tangent == midtangent at every point on the curve
        // except the cusp points.) Chop in between both cusps instead, if any.
        // There can be up to two cusps on a flat line, both where the tangent
        // is perpendicular to the starting tangent:
        //
        //     tangent dot tan0 == 0
        //
        coeffs = C_x * tan0.x + C_y * tan0.y;
        a = coeffs[0];
        b = coeffs[1];
        if (a != 0)
        {
            // We want the point in between both cusps. The midpoint of:
            //
            //     (-b +/- sqrt(b^2 - 4*a*c)) / (2*a)
            //
            // Is equal to:
            //
            //     -b / (2*a)
            T = -b / (2 * a);
        }
        if (!(T > 0 && T < 1))
        { // Use "!(positive_logic)" so T=NaN will take this branch.
            // Either the curve is a flat line with no rotation or FP precision
            // failed us. Chop at .5.
            T = .5;
        }
        return T;
    }
}

// Slices paths into sliver-size contours shaped like ice cream cones.
class MandolineSlicer
{
public:
    MandolineSlicer(Vec2D anchorPt) { this->reset(anchorPt); }

    void reset(Vec2D anchorPt)
    {
        fPath = Path();
        fPath->fillRule(FillRule::evenOdd);
        fLastPt = fAnchorPt = anchorPt;
    }

    void sliceLine(Vec2D pt, int numSubdivisions)
    {
        if (numSubdivisions <= 0)
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
        this->sliceLine(midpt, numSubdivisions - 1);
        this->sliceLine(pt, numSubdivisions - 1);
    }

    void sliceQuadratic(Vec2D p1, Vec2D p2, int numSubdivisions)
    {
        if (numSubdivisions <= 0)
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
        float T = SkFindQuadMidTangent(
            std::array<Vec2D, 3>{fAnchorPt, p1, p2}.data());
        if (0 == T)
        {
            return;
        }
        Vec2D P[4] = {fLastPt,
                      Vec2D::lerp(fLastPt, p1, 2 / 3.f),
                      Vec2D::lerp(p2, p1, 2 / 3.f),
                      p2},
              PP[7];
        ChopCubicAt(P, PP, T);

        this->sliceCubic(PP[1], PP[2], PP[3], numSubdivisions - 1);
        this->sliceCubic(PP[4], PP[5], PP[6], numSubdivisions - 1);
    }

    void sliceCubic(Vec2D p1, Vec2D p2, Vec2D p3, int numSubdivisions)
    {
        if (numSubdivisions <= 0)
        {
            fPath->moveTo(fAnchorPt.x, fAnchorPt.y);
            fPath->lineTo(fLastPt.x, fLastPt.y);
            fPath->cubicTo(p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
            fPath->close();
            fLastPt = p3;
            return;
        }
        float T = SkFindCubicMidTangent(
            std::array<Vec2D, 4>{fAnchorPt, p1, p2, p3}.data());
        if (0 == T)
        {
            return;
        }
        Vec2D P[4] = {fLastPt, p1, p2, p3}, PP[7];
        ChopCubicAt(P, PP, T);
        this->sliceCubic(PP[1], PP[2], PP[3], numSubdivisions - 1);
        this->sliceCubic(PP[4], PP[5], PP[6], numSubdivisions - 1);
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
    MandolineGM() : GM(560, 475, "mandoline") {}

protected:
    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(Renderer* renderer) override
    {
        int subdivisions = 12;
        if (gpu::RenderContext* renderContext =
                TestingWindow::Get()->renderContext())
        {
            if (renderContext->frameInterlockMode() ==
                    gpu::InterlockMode::atomics ||
                renderContext->frameInterlockMode() ==
                    gpu::InterlockMode::clockwiseAtomic)
            {
                // Atomic mode uses 7:9 fixed point and clockwiseAtomic uses
                // 7:8, so the winding number breaks if a shape has more than
                // +/-64 levels of self overlap at any point.
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
        mandoline.reset(
            {-cosf(degreesToRadians(-60)), sinf(degreesToRadians(-60))});
        mandoline.sliceQuadratic(
            {-2, 0},
            {-cosf(degreesToRadians(60)), sinf(degreesToRadians(60))},
            subdivisions);
        mandoline.sliceQuadratic(
            {-cosf(degreesToRadians(120)) * 2, sinf(degreesToRadians(120)) * 2},
            {1, 0},
            subdivisions);
        mandoline.sliceLine({0, 0}, subdivisions);
        mandoline.sliceLine(
            {-cosf(degreesToRadians(-60)), sinf(degreesToRadians(-60))},
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
            float theta1 = 2 * PI / nquads * (i + .5f);
            float theta2 = 2 * PI / nquads * (i + 1);
            mandoline.sliceQuadratic({cosf(theta1) * 2, sinf(theta1) * 2},
                                     {cosf(theta2), sinf(theta2)},
                                     subdivisions);
        }
        renderer->drawPath(mandoline.path(), paint);
        renderer->restore();
    }
};

GMREGISTER_SLOW(return new MandolineGM;)

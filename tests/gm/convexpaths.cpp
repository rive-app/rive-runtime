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
#include "common/rand.hpp"

using namespace rivegm;
using namespace rive;

namespace
{

class SkDoOnce
{
public:
    SkDoOnce() { fDidOnce = false; }

    bool alreadyDone() const { return fDidOnce; }
    void accomplished()
    {
        assert(!fDidOnce);
        fDidOnce = true;
    }

private:
    bool fDidOnce;
};

class ConvexPathsGM : public GM
{
public:
    ConvexPathsGM() : GM(1200, 1100, "convexpaths") {}

private:
    SkDoOnce fOnce;

    void makePaths()
    {

        if (fOnce.alreadyDone())
        {
            return;
        }
        fOnce.accomplished();

        PathBuilder b;
        fPaths.push_back(b.moveTo(0, 0).quadTo(50, 100, 0, 100).lineTo(0, 0).detach());

        fPaths.push_back(b.moveTo(0, 50).quadTo(50, 0, 100, 50).quadTo(50, 100, 0, 50).detach());

        fPaths.push_back(PathBuilder::Rect({0, 0, 100, 100}, rivegm::PathDirection::cw));
        fPaths.push_back(PathBuilder::Rect({0, 0, 100, 100}, rivegm::PathDirection::ccw));
        fPaths.push_back(PathBuilder::Oval({0, 0, 100, 100}, rivegm::PathDirection::cw));
        fPaths.push_back(PathBuilder::Oval({0, 0, 50, 100}, rivegm::PathDirection::cw));
        fPaths.push_back(PathBuilder::Oval({0, 0, 100, 5}, rivegm::PathDirection::ccw));
        fPaths.push_back(PathBuilder::Oval({0, 0, 1, 100}, rivegm::PathDirection::ccw));
        fPaths.push_back(PathBuilder::RRect({0, 0, 100, 100}, 40, 20));

        // large number of points
        constexpr static int kLength = 100;
        constexpr static int kPtsPerSide = (1 << 12);
        b.moveTo(0, 0);
        for (int i = 1; i < kPtsPerSide; ++i)
        { // skip the first point due to moveTo.
            b.lineTo(kLength * (float)(i) / kPtsPerSide, 0);
        }
        for (int i = 0; i < kPtsPerSide; ++i)
        {
            b.lineTo(kLength, kLength * (float)(i) / kPtsPerSide);
        }
        for (int i = kPtsPerSide; i > 0; --i)
        {
            b.lineTo(kLength * (float)(i) / kPtsPerSide, kLength);
        }
        for (int i = kPtsPerSide; i > 0; --i)
        {
            b.lineTo(0, kLength * (float)(i) / kPtsPerSide);
        }
        fPaths.push_back(b.detach());

        // shallow diagonals
        fPaths.push_back(b.moveTo(0, 0).lineTo(100, 1).lineTo(98, 100).lineTo(3, 96).detach());
        fPaths.push_back(PathBuilder::Oval({0, 0, 50, 100}));

        // cubics
        fPaths.push_back(b.cubicTo(1, 1, 10, 90, 0, 100).detach());
        fPaths.push_back(b.cubicTo(100, 50, 20, 100, 0, 0).detach());

        // path that has a cubic with a repeated first control point and
        // a repeated last control point.
        fPaths.push_back(b.moveTo(10, 10)
                             .cubicTo(10, 10, 10, 0, 20, 0)
                             .lineTo(40, 0)
                             .cubicTo(40, 0, 50, 0, 50, 10)
                             .detach());

        // path that has two cubics with repeated middle control points.
        fPaths.push_back(b.moveTo(10, 10)
                             .cubicTo(10, 0, 10, 0, 20, 0)
                             .lineTo(40, 0)
                             .cubicTo(50, 0, 50, 0, 50, 10)
                             .detach());

        // cubic where last three points are almost a line
        fPaths.push_back(
            b.moveTo(0, 228.0f / 8)
                .cubicTo(628.0f / 8, 82.0f / 8, 1255.0f / 8, 141.0f / 8, 1883.0f / 8, 202.0f / 8)
                .detach());

        // flat cubic where the at end point tangents both point outward.
        fPaths.push_back(b.moveTo(10, 0).cubicTo(0, 1, 30, 1, 20, 0).detach());

        // flat cubic where initial tangent is in, end tangent out
        fPaths.push_back(b.moveTo(0, 0).cubicTo(10, 1, 30, 1, 20, 0).detach());

        // flat cubic where initial tangent is out, end tangent in
        fPaths.push_back(b.moveTo(10, 0).cubicTo(0, 1, 20, 1, 30, 0).detach());

        // triangle where one edge is a degenerate quad
        fPaths.push_back(b.moveTo(8.59375f, 45)
                             .quadTo(16.9921875f, 45, 31.25f, 45)
                             .lineTo(100, 100)
                             .lineTo(8.59375f, 45)
                             .detach());

        // triangle where one edge is a quad with a repeated point
        fPaths.push_back(b.moveTo(0, 25).lineTo(50, 0).quadTo(50, 50, 50, 50).detach());

        // triangle where one edge is a cubic with a 2x repeated point
        fPaths.push_back(b.moveTo(0, 25).lineTo(50, 0).cubicTo(50, 0, 50, 50, 50, 50).detach());

        // triangle where one edge is a quad with a nearly repeated point
        fPaths.push_back(b.moveTo(0, 25).lineTo(50, 0).quadTo(50, 49.95f, 50, 50).detach());

        // triangle where one edge is a cubic with a 3x nearly repeated point
        fPaths.push_back(
            b.moveTo(0, 25).lineTo(50, 0).cubicTo(50, 49.95f, 50, 49.97f, 50, 50).detach());

        // triangle where there is a point degenerate cubic at one corner
        fPaths.push_back(
            b.moveTo(0, 25).lineTo(50, 0).lineTo(50, 50).cubicTo(50, 50, 50, 50, 50, 50).detach());

        // point line
        fPaths.push_back(b.moveTo(50, 50).lineTo(50, 50).detach());

        // point quad
        fPaths.push_back(b.moveTo(50, 50).quadTo(50, 50, 50, 50).detach());

        // point cubic
        fPaths.push_back(b.moveTo(50, 50).cubicTo(50, 50, 50, 50, 50, 50).detach());

        // moveTo only paths
        fPaths.push_back(
            b.moveTo(0, 0).moveTo(0, 0).moveTo(1, 1).moveTo(1, 1).moveTo(10, 10).detach());

        fPaths.push_back(b.moveTo(0, 0).moveTo(0, 0).detach());

        // line degenerate
        fPaths.push_back(b.lineTo(100, 100).detach());
        fPaths.push_back(b.quadTo(100, 100, 0, 0).detach());
        fPaths.push_back(b.quadTo(100, 100, 50, 50).detach());
        fPaths.push_back(b.quadTo(50, 50, 100, 100).detach());
        fPaths.push_back(b.cubicTo(0, 0, 0, 0, 100, 100).detach());

        // skbug.com/8928
        fPaths.push_back(Path());
        fPaths.back()->addRenderPath(
            b.moveTo(16.875f, 192.594f)
                .cubicTo(45.625f, 192.594f, 74.375f, 192.594f, 103.125f, 192.594f)
                .cubicTo(88.75f, 167.708f, 74.375f, 142.823f, 60, 117.938f)
                .cubicTo(45.625f, 142.823f, 31.25f, 167.708f, 16.875f, 192.594f)
                .close()
                .detach(),
            rive::Mat2D(0.1f, 0, 0, 0.115207f, -1, -2.64977f));

        // small circle. This is listed last so that it has device coords far
        // from the origin (small area relative to x,y values).
        fPaths.push_back(PathBuilder::Oval({-1.2f, -1.2f, 1.2f, 1.2f}, rivegm::PathDirection::ccw));
    }

    ColorInt clearColor() const override { return 0xFF000000; }

    void onDraw(Renderer* renderer) override
    {
        this->makePaths();

        Paint paint;
        Rand rand;
        renderer->translate(20, 20);

        // As we've added more paths this has gotten pretty big. Scale the whole thing down.
        renderer->scale(2.0f / 3, 2.0f / 3);

        for (size_t i = 0; i < fPaths.size(); ++i)
        {
            renderer->save();
            // position the path, and make it at off-integer coords.
            renderer->translate(200.0f * (i % 5) + 1.0f / 10, 200.0f * (int)(i / 5) + 9.0f / 10);
            ColorInt color = rand.u32();
            color |= 0xff000000;
            paint->color(color);
#if 0 // This hitting on 32bit Linux builds for some paths. Temporarily disabling while it is
      // debugged.
            SkASSERT(fPaths[i].isConvex());
#endif
            renderer->drawPath(fPaths[i], paint);
            renderer->restore();
        }
    }

    std::vector<Path> fPaths;
};
} // namespace

GMREGISTER(return new ConvexPathsGM;)

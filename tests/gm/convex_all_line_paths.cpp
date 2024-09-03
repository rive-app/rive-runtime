/*
 * Copyright 2015 Google Inc.
 * Copyright 2022 Rive
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/renderer.hpp"
#include "skia/include/core/SkPoint.h"

using namespace rivegm;
using namespace rive;

static void create_ngon(int n, SkPoint* pts, SkScalar width, SkScalar height)
{
    float angleStep = 360.0f / n, angle = 0.0f;
    if ((n % 2) == 1)
    {
        angle = angleStep / 2.0f;
    }

    for (int i = 0; i < n; ++i)
    {
        pts[i].fX = -SkScalarSin(SkDegreesToRadians(angle)) * width;
        pts[i].fY = SkScalarCos(SkDegreesToRadians(angle)) * height;
        angle += angleStep;
    }
}

namespace ConvexLineOnlyData
{
// narrow rect
const SkPoint gPoints0[] = {{-1.5f, -50.0f}, {1.5f, -50.0f}, {1.5f, 50.0f}, {-1.5f, 50.0f}};
// narrow rect on an angle
const SkPoint gPoints1[] = {{-50.0f, -49.0f}, {-49.0f, -50.0f}, {50.0f, 49.0f}, {49.0f, 50.0f}};
// trap - narrow on top - wide on bottom
const SkPoint gPoints2[] = {{-10.0f, -50.0f}, {10.0f, -50.0f}, {50.0f, 50.0f}, {-50.0f, 50.0f}};
// wide skewed rect
const SkPoint gPoints3[] = {{-50.0f, -50.0f}, {0.0f, -50.0f}, {50.0f, 50.0f}, {0.0f, 50.0f}};
// thin rect with colinear-ish lines
const SkPoint gPoints4[] = {{-6.0f, -50.0f},
                            {4.0f, -50.0f},
                            {5.0f, -25.0f}, // remove if collinear diagonal points are not concave
                            {6.0f, 0.0f},
                            {5.0f, 25.0f}, // remove if collinear diagonal points are not concave
                            {4.0f, 50.0f},
                            {-4.0f, 50.0f}};
// degenerate
const SkPoint gPoints5[] = {{-0.025f, -0.025f},
                            {0.025f, -0.025f},
                            {0.025f, 0.025f},
                            {-0.025f, 0.025f}};
// Triangle in which the first point should fuse with last
const SkPoint gPoints6[] = {{-20.0f, -13.0f}, {-20.0f, -13.05f}, {20.0f, -13.0f}, {20.0f, 27.0f}};
// thin rect with colinear lines
const SkPoint gPoints7[] = {{-10.0f, -50.0f},
                            {10.0f, -50.0f},
                            {10.0f, -25.0f},
                            {10.0f, 0.0f},
                            {10.0f, 25.0f},
                            {10.0f, 50.0f},
                            {-10.0f, 50.0f}};
// capped teardrop
const SkPoint gPoints8[] = {{50.00f, 50.00f},
                            {0.00f, 50.00f},
                            {-15.45f, 47.55f},
                            {-29.39f, 40.45f},
                            {-40.45f, 29.39f},
                            {-47.55f, 15.45f},
                            {-50.00f, 0.00f},
                            {-47.55f, -15.45f},
                            {-40.45f, -29.39f},
                            {-29.39f, -40.45f},
                            {-15.45f, -47.55f},
                            {0.00f, -50.00f},
                            {50.00f, -50.00f}};
// teardrop
const SkPoint gPoints9[] = {{4.39f, 40.45f},
                            {-9.55f, 47.55f},
                            {-25.00f, 50.00f},
                            {-40.45f, 47.55f},
                            {-54.39f, 40.45f},
                            {-65.45f, 29.39f},
                            {-72.55f, 15.45f},
                            {-75.00f, 0.00f},
                            {-72.55f, -15.45f},
                            {-65.45f, -29.39f},
                            {-54.39f, -40.45f},
                            {-40.45f, -47.55f},
                            {-25.0f, -50.0f},
                            {-9.55f, -47.55f},
                            {4.39f, -40.45f},
                            {75.00f, 0.00f}};
// clipped triangle
const SkPoint gPoints10[] = {
    {-10.0f, -50.0f},
    {10.0f, -50.0f},
    {50.0f, 31.0f},
    {40.0f, 50.0f},
    {-40.0f, 50.0f},
    {-50.0f, 31.0f},
};

const SkPoint* gPoints[] = {
    gPoints0,
    gPoints1,
    gPoints2,
    gPoints3,
    gPoints4,
    gPoints5,
    gPoints6,
    gPoints7,
    gPoints8,
    gPoints9,
    gPoints10,
};

const size_t gSizes[] = {
    SK_ARRAY_COUNT(gPoints0),
    SK_ARRAY_COUNT(gPoints1),
    SK_ARRAY_COUNT(gPoints2),
    SK_ARRAY_COUNT(gPoints3),
    SK_ARRAY_COUNT(gPoints4),
    SK_ARRAY_COUNT(gPoints5),
    SK_ARRAY_COUNT(gPoints6),
    SK_ARRAY_COUNT(gPoints7),
    SK_ARRAY_COUNT(gPoints8),
    SK_ARRAY_COUNT(gPoints9),
    SK_ARRAY_COUNT(gPoints10),
};
static_assert(SK_ARRAY_COUNT(gSizes) == SK_ARRAY_COUNT(gPoints), "array_mismatch");
} // namespace ConvexLineOnlyData

// This GM is intended to exercise Ganesh's handling of convex line-only
// paths
class ConvexLineOnlyPathsGM : public GM
{
public:
    ConvexLineOnlyPathsGM() : GM(kGMWidth, kGMHeight, "convex-lineonly-ths") {}

protected:
    static Path GetPath(int index, rivegm::PathDirection dir)
    {
        std::unique_ptr<SkPoint[]> data(nullptr);
        const SkPoint* points;
        int numPts;
        if (index < (int)SK_ARRAY_COUNT(ConvexLineOnlyData::gPoints))
        {
            // manually specified
            points = ConvexLineOnlyData::gPoints[index];
            numPts = (int)ConvexLineOnlyData::gSizes[index];
        }
        else
        {
            // procedurally generated
            SkScalar width = (float)kMaxPathHeight / 2;
            SkScalar height = (float)kMaxPathHeight / 2;
            switch (index - SK_ARRAY_COUNT(ConvexLineOnlyData::gPoints))
            {
                case 0:
                    numPts = 3;
                    break;
                case 1:
                    numPts = 4;
                    break;
                case 2:
                    numPts = 5;
                    break;
                case 3: // squashed pentagon
                    numPts = 5;
                    width = (float)kMaxPathHeight / 5;
                    break;
                case 4:
                    numPts = 6;
                    break;
                case 5:
                    numPts = 8;
                    break;
                case 6: // squashed octogon
                    numPts = 8;
                    width = (float)kMaxPathHeight / 5;
                    break;
                case 7:
                    numPts = 20;
                    break;
                case 8:
                    numPts = 100;
                    break;
                default:
                    numPts = 3;
                    break;
            }

            data = std::make_unique<SkPoint[]>(numPts);

            create_ngon(numPts, data.get(), width, height);
            points = data.get();
        }

        PathBuilder builder;

        if (rivegm::PathDirection::cw == dir)
        {
            builder.moveTo(points[0].x(), points[0].y());
            for (int i = 1; i < numPts; ++i)
            {
                builder.lineTo(points[i].x(), points[i].y());
            }
        }
        else
        {
            builder.moveTo(points[numPts - 1].x(), points[numPts - 1].y());
            for (int i = numPts - 2; i >= 0; --i)
            {
                builder.lineTo(points[i].x(), points[i].y());
            }
        }

        builder.close();
        Path path = builder.detach();
#if 0
        // Each path this method returns should be convex, only composed of
        // lines, wound the right direction, and short enough to fit in one
        // of the GMs rows.
        SkASSERT(path.isConvex());
        SkASSERT(SkPath::kLine_SegmentMask == path.getSegmentMasks());
        SkPathFirstDirection actualDir = SkPathPriv::ComputeFirstDirection(path);
        SkASSERT(SkPathPriv::AsFirstDirection(dir) == actualDir);
        SkRect bounds = path.getBounds();
        SkASSERT(SkScalarNearlyEqual(bounds.centerX(), 0.0f));
        SkASSERT(bounds.height() <= kMaxPathHeight);
#endif
        return path;
    }

    // Draw a single path several times, shrinking it, flipping its direction
    // and changing its start vertex each time.
    void drawPath(Renderer* renderer, int index, SkPoint* offset)
    {

        SkPoint center;
        {
            Path path = GetPath(index, rivegm::PathDirection::cw);
            if (offset->fX + kMaxPathWidth /* path.getBounds().width() */ > kGMWidth)
            {
                offset->fX = 0;
                offset->fY += kMaxPathHeight;
            }
            center = {offset->fX + SkScalarHalf(kMaxPathHeight /* path.getBounds().width() */),
                      offset->fY};
            offset->fX += kMaxPathWidth /* path.getBounds().width() */;
        }

        const ColorInt colors[2] = {0xff00ff00, 0xff0000ff};
        const rivegm::PathDirection dirs[2] = {rivegm::PathDirection::cw,
                                               rivegm::PathDirection::ccw};
        const float scales[] = {1.0f, 0.75f, 0.5f, 0.25f, 0.1f, 0.01f, 0.001f};

        Paint paint;

        for (size_t i = 0; i < SK_ARRAY_COUNT(scales); ++i)
        {
            Path path = GetPath(index, dirs[i % 2]);
            renderer->save();
            renderer->translate(center.fX, center.fY);
            renderer->scale(scales[i], scales[i]);
            paint->color(colors[i % 2]);
            renderer->drawPath(path, paint);
            renderer->restore();
        }
    }

    void onDraw(Renderer* renderer) override
    {
        // the right edge of the last drawn path
        SkPoint offset = {0, SkScalarHalf(kMaxPathHeight)};
        for (int i = 0; i < kNumPaths; ++i)
        {
            this->drawPath(renderer, i, &offset);
        }

#if 0
        {
            // Repro for crbug.com/472723 (Missing AA on portions of graphic with GPU
            // rasterization)

            Paint p;
            Path p1 = PathBuilder()
                          .moveTo(60.8522949f, 364.671021f)
                          .moveTo(59.4380493f, 364.671021f)
                          .moveTo(385.414276f, 690.647217f)
                          .moveTo(386.121399f, 689.940125f)
                          .detach();
            renderer->save();
            renderer->translate(356.0f, 50.0f);
            renderer->drawPath(p1, p);
            renderer->restore();

            // Repro for crbug.com/869172 (SVG path incorrectly simplified when using GPU
            // Rasterization). This will only draw anything in the stroke-and-fill version.
            Path p2 = PathBuilder()
                          .moveTo(10.f, 0.f)
                          .lineTo(38.f, 0.f)
                          .lineTo(66.f, 0.f)
                          .lineTo(94.f, 0.f)
                          .lineTo(122.f, 0.f)
                          .lineTo(150.f, 0.f)
                          .lineTo(150.f, 0.f)
                          .lineTo(122.f, 0.f)
                          .lineTo(94.f, 0.f)
                          .lineTo(66.f, 0.f)
                          .lineTo(38.f, 0.f)
                          .lineTo(10.f, 0.f)
                          .close()
                          .detach();
            renderer->save();
            renderer->translate(0.0f, 500.0f);
            renderer->drawPath(p2, p);
            renderer->restore();

            // Repro for crbug.com/856137. This path previously caused GrAAConvexTessellator to
            // turn inset rings into outsets when adjacent bisector angles converged outside the
            // previous ring due to accumulated error.
            Path p3 = PathBuilder()
                          .moveTo(1184.96f, 982.557f)
                          .lineTo(1183.71f, 982.865f)
                          .lineTo(1180.99f, 982.734f)
                          .lineTo(1178.5f, 981.541f)
                          .lineTo(1176.35f, 979.367f)
                          .lineTo(1178.94f, 938.854f)
                          .lineTo(1181.35f, 936.038f)
                          .lineTo(1183.96f, 934.117f)
                          .lineTo(1186.67f, 933.195f)
                          .lineTo(1189.36f, 933.342f)
                          .lineTo(1191.58f, 934.38f)
                          .close()
                          .detach();
            p3->fillRule(FillRule::evenOdd);
            renderer->save();
            renderer->transform(Mat2D(0.0893210843f, 0, 0, 0.0893210843f, 79.1197586f, 300));
            renderer->drawPath(p3, p);
            renderer->restore();
        }
#endif
    }

private:
    inline static constexpr int kStrokeWidth = 10;
    inline static constexpr int kNumPaths = 20;
    inline static constexpr int kMaxPathWidth = 130;
    inline static constexpr int kMaxPathHeight = 130;
    inline static constexpr int kGMWidth = 540;
    inline static constexpr int kGMHeight = 680;

    using INHERITED = GM;
};

//////////////////////////////////////////////////////////////////////////////

GMREGISTER(return new ConvexLineOnlyPathsGM;)

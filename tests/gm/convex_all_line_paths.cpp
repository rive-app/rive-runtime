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
#include "rive/math/vec2d.hpp"
#include "rive/math/math_types.hpp"

using namespace rivegm;
using namespace rive;

static void create_ngon(int n, Vec2D* pts, float width, float height)
{
    float angleStep = 360.0f / n, angle = 0.0f;
    if ((n % 2) == 1)
    {
        angle = angleStep / 2.0f;
    }

    for (int i = 0; i < n; ++i)
    {
        pts[i].x = -sinf(rive::math::degrees_to_radians(angle)) * width;
        pts[i].y = cosf(rive::math::degrees_to_radians(angle)) * height;
        angle += angleStep;
    }
}

namespace ConvexLineOnlyData
{
// narrow rect
const Vec2D gPoints0[] = {{-1.5f, -50.0f},
                          {1.5f, -50.0f},
                          {1.5f, 50.0f},
                          {-1.5f, 50.0f}};
// narrow rect on an angle
const Vec2D gPoints1[] = {{-50.0f, -49.0f},
                          {-49.0f, -50.0f},
                          {50.0f, 49.0f},
                          {49.0f, 50.0f}};
// trap - narrow on top - wide on bottom
const Vec2D gPoints2[] = {{-10.0f, -50.0f},
                          {10.0f, -50.0f},
                          {50.0f, 50.0f},
                          {-50.0f, 50.0f}};
// wide skewed rect
const Vec2D gPoints3[] = {{-50.0f, -50.0f},
                          {0.0f, -50.0f},
                          {50.0f, 50.0f},
                          {0.0f, 50.0f}};
// thin rect with colinear-ish lines
const Vec2D gPoints4[] = {
    {-6.0f, -50.0f},
    {4.0f, -50.0f},
    {5.0f, -25.0f}, // remove if collinear diagonal points are not concave
    {6.0f, 0.0f},
    {5.0f, 25.0f}, // remove if collinear diagonal points are not concave
    {4.0f, 50.0f},
    {-4.0f, 50.0f}};
// degenerate
const Vec2D gPoints5[] = {{-0.025f, -0.025f},
                          {0.025f, -0.025f},
                          {0.025f, 0.025f},
                          {-0.025f, 0.025f}};
// Triangle in which the first point should fuse with last
const Vec2D gPoints6[] = {{-20.0f, -13.0f},
                          {-20.0f, -13.05f},
                          {20.0f, -13.0f},
                          {20.0f, 27.0f}};
// thin rect with colinear lines
const Vec2D gPoints7[] = {{-10.0f, -50.0f},
                          {10.0f, -50.0f},
                          {10.0f, -25.0f},
                          {10.0f, 0.0f},
                          {10.0f, 25.0f},
                          {10.0f, 50.0f},
                          {-10.0f, 50.0f}};
// capped teardrop
const Vec2D gPoints8[] = {{50.00f, 50.00f},
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
const Vec2D gPoints9[] = {{4.39f, 40.45f},
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
const Vec2D gPoints10[] = {
    {-10.0f, -50.0f},
    {10.0f, -50.0f},
    {50.0f, 31.0f},
    {40.0f, 50.0f},
    {-40.0f, 50.0f},
    {-50.0f, 31.0f},
};

const Vec2D* gPoints[] = {
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
    std::size(gPoints0),
    std::size(gPoints1),
    std::size(gPoints2),
    std::size(gPoints3),
    std::size(gPoints4),
    std::size(gPoints5),
    std::size(gPoints6),
    std::size(gPoints7),
    std::size(gPoints8),
    std::size(gPoints9),
    std::size(gPoints10),
};
static_assert(std::size(gSizes) == std::size(gPoints), "array_mismatch");
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
        std::unique_ptr<Vec2D[]> data(nullptr);
        const Vec2D* points;
        int numPts;
        if (index < (int)std::size(ConvexLineOnlyData::gPoints))
        {
            // manually specified
            points = ConvexLineOnlyData::gPoints[index];
            numPts = (int)ConvexLineOnlyData::gSizes[index];
        }
        else
        {
            // procedurally generated
            float width = (float)kMaxPathHeight / 2;
            float height = (float)kMaxPathHeight / 2;
            switch (index - std::size(ConvexLineOnlyData::gPoints))
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

            data = std::make_unique<Vec2D[]>(numPts);

            create_ngon(numPts, data.get(), width, height);
            points = data.get();
        }

        PathBuilder builder;

        if (rivegm::PathDirection::cw == dir)
        {
            builder.moveTo(points[0].x, points[0].y);
            for (int i = 1; i < numPts; ++i)
            {
                builder.lineTo(points[i].x, points[i].y);
            }
        }
        else
        {
            builder.moveTo(points[numPts - 1].x, points[numPts - 1].y);
            for (int i = numPts - 2; i >= 0; --i)
            {
                builder.lineTo(points[i].x, points[i].y);
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
        SkASSERT(floatNearlyEqual(bounds.centerX(), 0.0f));
        SkASSERT(bounds.height() <= kMaxPathHeight);
#endif
        return path;
    }

    // Draw a single path several times, shrinking it, flipping its direction
    // and changing its start vertex each time.
    void drawPath(Renderer* renderer, int index, Vec2D* offset)
    {

        Vec2D center;
        {
            Path path = GetPath(index, rivegm::PathDirection::cw);
            if (offset->x + kMaxPathWidth /* path.getBounds().width() */ >
                kGMWidth)
            {
                offset->x = 0;
                offset->y += kMaxPathHeight;
            }
            center = {offset->x +
                          0.5f *
                              (kMaxPathHeight /* path.getBounds().width() */),
                      offset->y};
            offset->x += kMaxPathWidth /* path.getBounds().width() */;
        }

        const ColorInt colors[2] = {0xff00ff00, 0xff0000ff};
        const rivegm::PathDirection dirs[2] = {rivegm::PathDirection::cw,
                                               rivegm::PathDirection::ccw};
        const float scales[] = {1.0f, 0.75f, 0.5f, 0.25f, 0.1f, 0.01f, 0.001f};

        Paint paint;

        for (size_t i = 0; i < std::size(scales); ++i)
        {
            Path path = GetPath(index, dirs[i % 2]);
            renderer->save();
            renderer->translate(center.x, center.y);
            renderer->scale(scales[i], scales[i]);
            paint->color(colors[i % 2]);
            renderer->drawPath(path, paint);
            renderer->restore();
        }
    }

    void onDraw(Renderer* renderer) override
    {
        // the right edge of the last drawn path
        Vec2D offset = {0, 0.5f * (kMaxPathHeight)};
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

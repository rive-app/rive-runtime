/*
 * Copyright 2018 Google Inc.
 * Copyright 2022 Rive
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/math/bezier_utils.hpp"
#include "rive/renderer.hpp"
#include "common/rand.hpp"

using namespace rivegm;
using namespace rive;

static constexpr float kStrokeWidth = 30;
static constexpr int kCellSize = 200;
static constexpr int kNumCols = 5;
static constexpr int kNumRows = 5;
static constexpr int kTestWidth = kNumCols * kCellSize;
static constexpr int kTestHeight = kNumRows * kCellSize;

enum class CellFillMode
{
    kStretch,
    kCenter
};

struct TrickyCubic
{
    Vec2D fPoints[4];
    int fNumPts;
    CellFillMode fFillMode;
    float fScale = 1;
};

// This is a compilation of cubics that have given strokers grief. Feel free to
// add more.
static const TrickyCubic kTrickyCubics[] = {
    {{{122, 737}, {348, 553}, {403, 761}, {400, 760}},
     4,
     CellFillMode::kStretch},
    {{{244, 520}, {244, 518}, {1141, 634}, {394, 688}},
     4,
     CellFillMode::kStretch},
    {{{550, 194}, {138, 130}, {1035, 246}, {288, 300}},
     4,
     CellFillMode::kStretch},
    {{{226, 733}, {556, 779}, {-43, 471}, {348, 683}},
     4,
     CellFillMode::kStretch},
    {{{268, 204}, {492, 304}, {352, 23}, {433, 412}},
     4,
     CellFillMode::kStretch},
    {{{172, 480}, {396, 580}, {256, 299}, {338, 677}},
     4,
     CellFillMode::kStretch},
    {{{731, 340}, {318, 252}, {1026, -64}, {367, 265}},
     4,
     CellFillMode::kStretch},
    {{{475, 708}, {62, 620}, {770, 304}, {220, 659}},
     4,
     CellFillMode::kStretch},
    {{{0, 0}, {128, 128}, {128, 0}, {0, 128}},
     4,
     CellFillMode::kCenter}, // Perfect cusp
    {{{0, .01f}, {128, 127.999f}, {128, .01f}, {0, 127.99f}},
     4,
     CellFillMode::kCenter}, // Near-cusp
    {{{0, -.01f}, {128, 128.001f}, {128, -.01f}, {0, 128.001f}},
     4,
     CellFillMode::kCenter}, // Near-cusp
    {{{0, 0}, {0, -10}, {0, -10}, {0, 10}},
     4,
     CellFillMode::kCenter,
     1.098283f}, // Flat line with 180
    {{{10, 0}, {0, 0}, {20, 0}, {10, 0}},
     4,
     CellFillMode::kStretch}, // Flat line with 2 180s
    {{{39, -39}, {40, -40}, {40, -40}, {0, 0}},
     4,
     CellFillMode::kStretch}, // Flat diagonal with 180
    {{{40, 40}, {0, 0}, {200, 200}, {0, 0}},
     4,
     CellFillMode::kStretch}, // Diag w/ an internal 180
    {{{0, 0}, {1e-2f, 0}, {-1e-2f, 0}, {0, 0}},
     4,
     CellFillMode::kCenter}, // Circle
    {{{400.75f, 100.05f},
      {400.75f, 100.05f},
      {100.05f, 300.95f},
      {100.05f, 300.95f}},
     4,
     CellFillMode::kStretch}, // Flat line with no turns
    {{{0.5f, 0}, {0, 0}, {20, 0}, {10, 0}},
     4,
     CellFillMode::kStretch}, // Flat line with 2 180s
    {{{10, 0}, {0, 0}, {10, 0}, {10, 0}},
     4,
     CellFillMode::kStretch}, // Flat line with a 180
    {{{1, 1}, {2, 1}, {1, 1}, {1, std::numeric_limits<float>::quiet_NaN()}},
     3,
     CellFillMode::kStretch}, // Flat QUAD with a cusp
    {{{1, 1},
      {100, 1},
      {25, 1},
      {.3f, std::numeric_limits<float>::quiet_NaN()}},
     3,
     CellFillMode::kStretch}, // Flat CONIC with a cusp
    {{{1, 1},
      {70, 1},
      {25, 1},
      {1.5f, std::numeric_limits<float>::quiet_NaN()}},
     3,
     CellFillMode::kStretch}, // Flat CONIC with a cusp
};

static AABB calc_tight_cubic_bounds(const Vec2D P[4], int depth = 5)
{
    if (0 == depth)
    {
        AABB bounds;
        bounds.minX =
            std::min(std::min(P[0].x, P[1].x), std::min(P[2].x, P[3].x));
        bounds.minY =
            std::min(std::min(P[0].y, P[1].y), std::min(P[2].y, P[3].y));
        bounds.maxX =
            std::max(std::max(P[0].x, P[1].x), std::max(P[2].x, P[3].x));
        bounds.maxY =
            std::max(std::max(P[0].y, P[1].y), std::max(P[2].y, P[3].y));
        return bounds;
    }

    Vec2D chopped[7];
    math::chop_cubic_at(P, chopped, .5f);
    AABB bounds0 = calc_tight_cubic_bounds(chopped, depth - 1);
    AABB bounds1 = calc_tight_cubic_bounds(chopped + 3, depth - 1);
    if (bounds1.isEmptyOrNaN())
    {
        return bounds0;
    }
    if (bounds0.isEmptyOrNaN())
    {
        return bounds1;
    }
    bounds0.expand(bounds1);
    return bounds0;
}

static Vec2D lerp(const Vec2D& a, const Vec2D& b, float T)
{
    assert(1 != T); // The below does not guarantee lerp(a, b, 1) === b.
    return (b - a) * T + a;
}

enum class FillMode
{
    kCenter,
    kScale
};

class TrickyCubicsGM : public GM
{
public:
    TrickyCubicsGM(StrokeCap cap, StrokeJoin join, float feather) :
        GM(kTestWidth, kTestHeight),
        m_Cap(cap),
        m_Join(join),
        m_feather(feather)
    {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* renderer) override
    {
        Rand rand;

        Paint strokePaint;
        strokePaint->style(RenderPaintStyle::stroke);
        strokePaint->thickness(kStrokeWidth);
        strokePaint->cap(m_Cap);
        strokePaint->join(m_Join);

        for (size_t i = 0; i < std::size(kTrickyCubics); ++i)
        {
            auto [originalPts, numPts, fillMode, scale] = kTrickyCubics[i];

            assert(numPts <= 4);
            Vec2D p[4];
            memcpy(p, originalPts, sizeof(Vec2D) * numPts);
            for (int j = 0; j < numPts; ++j)
            {
                p[j] *= scale;
            }

            auto cellRect = AABB::fromLTWH((i % kNumCols) * kCellSize,
                                           (int)(i / kNumCols) * kCellSize,
                                           kCellSize,
                                           kCellSize);

            AABB strokeBounds;
            if (numPts == 4)
            {
                strokeBounds = calc_tight_cubic_bounds(p);
            }
            else
            {
                assert(numPts == 3);
                Vec2D asCubic[4] = {p[0],
                                    lerp(p[0], p[1], 2 / 3.f),
                                    lerp(p[1], p[2], 1 / 3.f),
                                    p[2]};
                strokeBounds = calc_tight_cubic_bounds(asCubic);
            }
            strokeBounds = strokeBounds.inset(-kStrokeWidth, -kStrokeWidth);

            Mat2D matrix;
            if (fillMode == CellFillMode::kStretch)
            {
                matrix = rive::computeAlignment(Fit::contain,
                                                Alignment::center,
                                                cellRect,
                                                strokeBounds);
            }
            else
            {
                matrix = Mat2D::fromTranslate(
                    cellRect.minX + kStrokeWidth +
                        (cellRect.width() - strokeBounds.width()) / 2,
                    cellRect.minY + kStrokeWidth +
                        (cellRect.height() - strokeBounds.height()) / 2);
            }

            renderer->save();
            renderer->transform(matrix);
            strokePaint->thickness(kStrokeWidth / matrix.findMaxScale());
            strokePaint->color(rand.u32() | 0xff808080);
            Path path;
            path->moveTo(p[0].x, p[0].y);
            if (numPts == 4)
            {
                path->cubicTo(p[1].x, p[1].y, p[2].x, p[2].y, p[3].x, p[3].y);
            }
            else
            {
                assert(numPts == 3);
                Vec2D c0 = p[0] + (p[1] - p[0]) * (2 / 3.f);
                Vec2D c1 = p[2] + (p[1] - p[2]) * (2 / 3.f);
                path->cubicTo(c0.x, c0.y, c1.x, c1.y, p[2].x, p[2].y);
            }
            if (m_feather != 0)
            {
                strokePaint->feather(m_feather / matrix.findMaxScale());
            }
            renderer->drawPath(path, strokePaint);
            renderer->restore();
        }
    }

private:
    StrokeCap m_Cap;
    StrokeJoin m_Join;
    float m_feather;
};

GMREGISTER(trickycubicstrokes,
           return new TrickyCubicsGM(StrokeCap::butt, StrokeJoin::miter, 0))
GMREGISTER(trickycubicstrokes_roundcaps,
           return new TrickyCubicsGM(StrokeCap::round, StrokeJoin::round, 0))
// Feathers ignore cap and join.
GMREGISTER(trickycubicstrokes_feather,
           return new TrickyCubicsGM(StrokeCap::butt, StrokeJoin::miter, 20))

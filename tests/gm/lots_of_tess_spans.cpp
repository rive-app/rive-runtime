/*
 * Copyright 2025 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/renderer.hpp"
#include <set>

using namespace rivegm;
using namespace rive;

constexpr static float R = 490;

static void add_circle(PathBuilder&,
                       float x,
                       float y,
                       float radius,
                       rivegm::PathDirection);

static void draw_spirograph(Renderer*,
                            RenderPaint*,
                            RenderPath* circles,
                            float2 translate,
                            float2 c,
                            int depth,
                            std::set<std::tuple<int, int>>* drawnCenters);

// Stress tests the renderer by trying to fit as many tessellation spans into a
// single flush as possible. This will hopefully detect any potential driver
// issues associated with tess span overload.
class LotsOfTessSpansGM : public GM
{
public:
    LotsOfTessSpansGM(RenderPaintStyle style) : GM(1024, 1024), m_style(style)
    {}

private:
    void onDraw(Renderer* renderer) override
    {
        PathBuilder pathBuilder;
        pathBuilder.fillRule(FillRule::clockwise);
        for (uint32_t i = (m_style == RenderPaintStyle::fill) ? 25 : 1; i < 50;
             ++i)
        {
            add_circle(pathBuilder, 0, 0, i * 10, rivegm::PathDirection::cw);
            if (m_style == RenderPaintStyle::fill)
            {
                add_circle(pathBuilder,
                           0,
                           0,
                           i * 10 - 1,
                           rivegm::PathDirection::ccw);
            }
        }
        Path circles = pathBuilder.detach();

        float2 translate = {512, 512};
        Paint paint;
        paint->style(m_style);
        // Use round joins because they use fewer segments when nearly flat, and
        // we want to generate as many tessellation *segments* (not spokes) as
        // possible in a single flush.
        paint->join(StrokeJoin::round);
        paint->color((m_style == RenderPaintStyle::fill) ? 0xff7500ff
                                                         : 0xfffe0089);
        renderer->drawPath(circles, paint);
        std::set<std::tuple<int, int>> drawnCenters;
        draw_spirograph(renderer,
                        paint,
                        circles,
                        translate,
                        float2{-R, 0},
                        3,
                        &drawnCenters);
        draw_spirograph(renderer,
                        paint,
                        circles,
                        translate,
                        float2{0, R},
                        3,
                        &drawnCenters);
        draw_spirograph(renderer,
                        paint,
                        circles,
                        translate,
                        float2{R, 0},
                        3,
                        &drawnCenters);
        draw_spirograph(renderer,
                        paint,
                        circles,
                        translate,
                        float2{0, -R},
                        3,
                        &drawnCenters);
    }

    const RenderPaintStyle m_style;
};

static void add_circle(PathBuilder& pathBuilder,
                       float x,
                       float y,
                       float radius,
                       rivegm::PathDirection dir)
{
    const int n = std::max(static_cast<int>(2 * math::PI * radius / 32), 3);
    pathBuilder.moveTo(x + radius, y);
    for (int i = 1; i <= n; ++i)
    {
        float c0 = 2 * math::PI * static_cast<float>(i - (2.f / 3.f)) / n;
        float c1 = 2 * math::PI * static_cast<float>(i - (1.f / 3.f)) / n;
        float theta = 2 * math::PI * static_cast<float>(i) / n;
        if (dir == rivegm::PathDirection::ccw)
        {
            c0 = -c0;
            c1 = -c1;
            theta = -theta;
        }
        // NOTE: this is not a true approximation of the arc. The segment is so
        // tiny that any simple cubic will work for our testing purposes. A true
        // arc approximation would take more complex math to figure out the
        // tangents on both ends, and then scale them such that the height of
        // the cubic matched the arc.
        pathBuilder.cubicTo(x + radius * cosf(c0),
                            y + radius * sinf(c0),
                            x + radius * cosf(c1),
                            y + radius * sinf(c1),
                            x + radius * cosf(theta),
                            y + radius * sinf(theta));
    }
    pathBuilder.close();
}

static void draw_spirograph(Renderer* renderer,
                            RenderPaint* paint,
                            RenderPath* circles,
                            float2 translate,
                            float2 c,
                            int depth,
                            std::set<std::tuple<int, int>>* drawnCenters)
{
    if (depth <= 0)
    {
        return;
    }

    translate += c;

    // Only draw the circles once, centered within any given pixel. When we draw
    // overlapping circles, coverage-based AA darkens but MSAA does not, leading
    // to divergence in golden images.
    int2 icenter = simd::cast<int>(simd::floor(translate + .5f));
    if (drawnCenters->insert({int(icenter.x), int(icenter.y)}).second)
    {
        AutoRestore ar(renderer, /*doSave=*/true);
        renderer->translate(translate.x, translate.y);
        renderer->drawPath(circles, paint);
    }

    // Next draw circles centered on both points of intersection between the
    // circle we just drew and the one before it!

    // y = mx + b
    float2 c0 = -c;
    if (fabsf(c.x) > fabsf(c.y))
    {
        c0 = c0.yx;
    }
    const float m = -c0.x / c0.y;
    const float b = simd::dot(c0, c0) / (2 * c0.y);

    // Ax^2 + Bx + C = 0
    const float A = 1 + m * m;
    const float B = 2 * m * b;
    const float C = b * b - R * R;

    float2 left;
    left.x = (-B + sqrtf(B * B - 4 * A * C)) / (2 * A);
    left.y = m * left.x + b;

    float2 right;
    right.x = (-B - sqrtf(B * B - 4 * A * C)) / (2 * A);
    right.y = m * right.x + b;

    if (fabsf(c.x) > fabsf(c.y))
    {
        left = left.yx;
        right = right.yx;
    }

    draw_spirograph(renderer,
                    paint,
                    circles,
                    translate,
                    left,
                    depth - 1,
                    drawnCenters);
    draw_spirograph(renderer,
                    paint,
                    circles,
                    translate,
                    right,
                    depth - 1,
                    drawnCenters);
}

GMREGISTER(lots_of_tess_spans_stroke,
           return new LotsOfTessSpansGM(RenderPaintStyle::stroke);)

// TODO: The "_fill" variant is wildly fill-heavy and triggers a
// VK_ERROR_DEVICE_LOST on some android devices. The error is probably
// appropriate for the capabilities of those devices, so we will need to figure
// out how to step around it if we want to use this test.
// GMREGISTER(lots_of_tess_spans_fill,
//            return new LotsOfTessSpansGM(RenderPaintStyle::fill);)

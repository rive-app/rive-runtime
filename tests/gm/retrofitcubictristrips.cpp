/*
 * Copyright 2023 Rive
 */

#include "gm.hpp"
#include "gr_inner_fan_triangulator.hpp"
#include "common/testing_window.hpp"
#include "rive/math/math_types.hpp"
#include "rive/renderer.hpp"
#include "rive/renderer/draw.hpp"
#include "rive/renderer/render_context.hpp"
#include "../src/rive_render_paint.hpp"
#include "../src/rive_render_path.hpp"
#include "../src/shaders/constants.glsl"

#include <cmath>

using namespace rivegm;
using namespace rive;
using namespace rive::gpu;

// Generates the vertices of a regular n-gon (circumradius r, centered at c,
// with vertex 0 at startAngleDegrees) in triangle-strip order. The zig-zag
// sequence 0, 1, n-1, 2, n-2, ... tessellates any convex polygon as a single
// strip.
static std::vector<Vec2D> regular_polygon_strip(int n,
                                                Vec2D c,
                                                float r,
                                                float startAngleDegrees)
{
    std::vector<Vec2D> perimeter;
    for (int i = 0; i < n; ++i)
    {
        float a = math::degrees_to_radians(startAngleDegrees) +
                  i * (2.f * math::PI / n);
        perimeter.push_back({c.x + r * std::cos(a), c.y + r * std::sin(a)});
    }
    std::vector<Vec2D> strip = {perimeter[0]};
    for (int lo = 1, hi = n - 1; lo <= hi;)
    {
        strip.push_back(perimeter[lo++]);
        if (lo <= hi)
        {
            strip.push_back(perimeter[hi--]);
        }
    }
    return strip;
}

// A 1x3 column of regular polygons, each drawn as a single retrofitted cubic
// triangle strip: an equilateral triangle, a square, then a regular pentagon.
static const std::vector<std::vector<Vec2D>> TriangleStrips = {
    regular_polygon_strip(3, {200, 200}, 150, -90.f),
    regular_polygon_strip(4, {200, 600}, 150, -135.f),
    regular_polygon_strip(5, {200, 1000}, 150, -90.f),
};

rcp<RiveRenderPath> make_nonempty_placeholder_path()
{
    auto path = make_rcp<RiveRenderPath>();
    path->moveTo(0, 0);
    return path;
}

class PushRetrofitTriStripsGMDraw : public PathDraw
{
public:
    PushRetrofitTriStripsGMDraw(RenderContext* context,
                                const Mat2D& matrix,
                                const RiveRenderPaint* paint) :
        PathDraw(FULLSCREEN_PIXEL_BOUNDS,
                 matrix,
                 nullptr,
                 make_nonempty_placeholder_path(),
                 context->frameDescriptor().clockwiseFillOverride
                     ? FillRule::clockwise
                     : FillRule::nonZero,
                 paint,
                 1.0f, // modulatedOpacity
                 SelectCoverageType(paint,
                                    1,
                                    context->platformFeatures(),
                                    context->frameInterlockMode()),
                 context->frameDescriptor())
    {
        m_resourceCounts.pathCount = 1;
        m_resourceCounts.contourCount = 1;
        m_resourceCounts.maxTessellatedSegmentCount = std::size(TriangleStrips);
        m_resourceCounts.outerCubicTessVertexCount =
            context->frameInterlockMode() != gpu::InterlockMode::msaa
                ? gpu::OuterCubicPatchSegmentSpanPlusJoin *
                      std::size(TriangleStrips) * 2
                : gpu::OuterCubicPatchSegmentSpanPlusJoin *
                      std::size(TriangleStrips);
        m_triangulator = context->make<GrInnerFanTriangulator>(
            RawPath(),
            AABB{},
            &context->perFrameAllocator());
    }

    void countSubpasses(const gpu::PlatformFeatures&) override
    {
        assert(m_prepassCount == 0);
        assert(m_subpassCount == 1);
    }

    bool allocateResources(RenderContext::LogicalFlush* flush) override
    {
        if (!PathDraw::allocateResources(flush))
        {
            return false;
        }
        return true;
    }

    gpu::DrawBatch* pushToRenderContext(RenderContext::LogicalFlush* flush,
                                        int subpassIndex) override
    {
        // Make sure the rawPath in our path reference hasn't changed since we
        // began holding!
        assert(m_rawPathMutationID == m_pathRef->getRawPathMutationID());
        assert(!m_pathRef->getRawPath().empty());
        assert(subpassIndex == 0);

        uint32_t tessVertexCount = math::lossless_numeric_cast<uint32_t>(
            m_resourceCounts.outerCubicTessVertexCount);
        if (tessVertexCount > 0)
        {
            m_pathID = flush->pushPath(this);

            uint32_t tessLocation =
                flush->allocateOuterCubicTessVertices(tessVertexCount);

            uint32_t forwardTessVertexCount, forwardTessLocation,
                mirroredTessVertexCount, mirroredTessLocation;
            if (m_contourDirections ==
                gpu::ContourDirections::reverseThenForward)
            {
                forwardTessVertexCount = mirroredTessVertexCount =
                    tessVertexCount / 2;
                forwardTessLocation = mirroredTessLocation =
                    tessLocation + tessVertexCount / 2;
            }
            else
            {
                assert(m_contourDirections == gpu::ContourDirections::forward);
                forwardTessVertexCount = tessVertexCount;
                forwardTessLocation = tessLocation;
                mirroredTessVertexCount = mirroredTessLocation = 0;
            }

            RenderContext::TessellationWriter tessWriter(
                flush,
                m_pathID,
                m_contourDirections,
                forwardTessVertexCount,
                forwardTessLocation,
                mirroredTessVertexCount,
                mirroredTessLocation);

            uint32_t contourID = tessWriter.pushContour(
                {0, 0},
                /*isStroke=*/false,
                /*closed=*/true,
                0 /* gpu::OuterCubicPatchSegmentSpan - 1 */);
            for (const auto& strips : TriangleStrips)
            {
                tessWriter.pushRetrofitCubicTriStrip(strips.data(),
                                                     strips.size(),
                                                     m_contourDirections,
                                                     contourID);
            }

            if (flush->frameDescriptor().clockwiseFillOverride)
            {
                m_drawContents |= gpu::DrawContents::clockwiseFill;
            }

            return &flush->pushOuterCubicsDraw(
                this,
                m_coverageType == CoverageType::msaa
                    ? gpu::DrawType::msaaOuterCubics
                    : gpu::DrawType::outerCurvePatches,
                tessVertexCount,
                tessLocation,
                gpu::ShaderMiscFlags::none);
        }
        return nullptr;
    }
};

// Checks that RenderContext properly draws triangle strips when using the
// RETROFIT_TRI_STRIP_CONTOUR_FLAG flag.
class RetrofitCubicTriStripsGM : public GM
{
public:
    RetrofitCubicTriStripsGM() : GM(800, 1200) {}

protected:
    virtual rive::ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(Renderer* renderer) override
    {
        gpu::RenderContext* renderContext =
            TestingWindow::Get()->renderContext();
        if (renderContext != nullptr)
        {
            RiveRenderPaint paint;
            paint.color(0xffffffff);
            DrawUniquePtr draw(
                renderContext->make<PushRetrofitTriStripsGMDraw>(renderContext,
                                                                 Mat2D(),
                                                                 &paint));
            bool success RIVE_MAYBE_UNUSED = renderContext->pushDraws(&draw, 1);
            assert(success);

            rive::gpu::RenderContext::FrameDescriptor frameDescriptor =
                renderContext->frameDescriptor();
            frameDescriptor.loadAction =
                rive::gpu::LoadAction::preserveRenderTarget;
#ifndef RIVE_ANDROID
            // Wireframe is not always supported on Android (e.g. Mali). Skip it
            // here so every Android device produces the same gold.
            frameDescriptor.wireframe = true;
#endif
            TestingWindow::Get()->flushPLSContext();
            renderContext->beginFrame(std::move(frameDescriptor));
            renderer->translate(400, 0);

            DrawUniquePtr wireframeDraw(
                renderContext->make<PushRetrofitTriStripsGMDraw>(
                    renderContext,
                    Mat2D::fromTranslation({400, 0}),
                    &paint));
            success = renderContext->pushDraws(&wireframeDraw, 1);
            assert(success);
        }
    }
};

GMREGISTER(retrofitcubictristrips, return new RetrofitCubicTriStripsGM;)

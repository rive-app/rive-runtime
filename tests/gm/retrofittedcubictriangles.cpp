/*
 * Copyright 2023 Rive
 */

#include "gm.hpp"
#include "gr_inner_fan_triangulator.hpp"
#include "common/testing_window.hpp"
#include "rive/renderer.hpp"
#include "rive/renderer/draw.hpp"
#include "rive/renderer/render_context.hpp"
#include "../src/rive_render_paint.hpp"
#include "../src/rive_render_path.hpp"
#include "../src/shaders/constants.glsl"

using namespace rivegm;
using namespace rive;
using namespace rive::gpu;

constexpr static std::array<Vec2D, 3> kTris[] = {
    {Vec2D{10, 10}, Vec2D{50, 10}, Vec2D{10, 120}},
    {Vec2D{10, 290}, Vec2D{290, 10}, Vec2D{270, 280}},
    {Vec2D{60, 60}, Vec2D{30, 190}, Vec2D{250, 20}},
};
constexpr static size_t kNumTriangles = sizeof(kTris) / sizeof(kTris[0]);

rcp<RiveRenderPath> make_nonempty_placeholder_path()
{
    auto path = make_rcp<RiveRenderPath>();
    path->moveTo(0, 0);
    return path;
}

class PushRetrofittedTrianglesGMDraw : public PathDraw
{
public:
    PushRetrofittedTrianglesGMDraw(RenderContext* context,
                                   const RiveRenderPaint* paint) :
        PathDraw(kFullscreenPixelBounds,
                 Mat2D(),
                 make_nonempty_placeholder_path(),
                 context->frameDescriptor().clockwiseFillOverride
                     ? FillRule::clockwise
                     : FillRule::nonZero,
                 paint,
                 SelectCoverageType(paint, context->frameInterlockMode()),
                 context->frameDescriptor())
    {
        m_resourceCounts.pathCount = 1;
        m_resourceCounts.contourCount = 1;
        m_resourceCounts.maxTessellatedSegmentCount = kNumTriangles;
        m_resourceCounts.outerCubicTessVertexCount =
            context->frameInterlockMode() != gpu::InterlockMode::msaa
                ? gpu::kOuterCurvePatchSegmentSpan * kNumTriangles * 2
                : gpu::kOuterCurvePatchSegmentSpan * kNumTriangles;
        m_triangulator = context->make<GrInnerFanTriangulator>(
            RawPath(),
            Mat2D(),
            GrTriangulator::Comparator::Direction::kHorizontal,
            FillRule::nonZero,
            &context->perFrameAllocator());
    }

    bool allocateResourcesAndSubpasses(
        RenderContext::LogicalFlush* flush) override
    {
        if (!PathDraw::allocateResourcesAndSubpasses(flush))
        {
            return false;
        }
        m_prepassCount = 0;
        m_subpassCount = 1;
        return true;
    }

    void pushToRenderContext(RenderContext::LogicalFlush* flush,
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

            // PushRetrofittedTrianglesGMDraw specific push to render
            uint32_t contourID = tessWriter.pushContour(
                {0, 0},
                /*isStroke=*/false,
                /*closed=*/true,
                0 /* gpu::kOuterCurvePatchSegmentSpan - 2 */);
            for (const auto& pts : kTris)
            {
                Vec2D tri[4] = {pts[0], pts[1], {0, 0}, pts[2]};
                tessWriter.pushCubic(tri,
                                     m_contourDirections,
                                     {0, 0},
                                     gpu::kOuterCurvePatchSegmentSpan - 1,
                                     1,
                                     1,
                                     contourID |
                                         RETROFITTED_TRIANGLE_CONTOUR_FLAG);
            }

            auto shaderMiscFlags = gpu::ShaderMiscFlags::none;
            if (flush->frameDescriptor().clockwiseFillOverride)
            {
                m_drawContents |= gpu::DrawContents::clockwiseFill;
            }
            flush->pushOuterCubicsDraw(this,
                                       tessVertexCount,
                                       tessLocation,
                                       shaderMiscFlags);
        }
    }
};

// Checks that RenderContext properly draws single triangles when using the
// "kRetrofittedTriangle" flag.
class RetrofittedCubicTrianglesGM : public GM
{
public:
    RetrofittedCubicTrianglesGM() : GM(300, 300, "retrofittedcubictriangles") {}

protected:
    void onDraw(Renderer* renderer) override
    {
        TestingWindow::Get()->endFrame(nullptr);
        gpu::RenderContext* renderContext =
            TestingWindow::Get()->renderContext();
        if (!renderContext)
        {
            TestingWindow::Get()->beginFrame({.clearColor = 0xffff0000});
        }
        else
        {
            TestingWindow::Get()->beginFrame({.clearColor = 0xff000000});
            RiveRenderPaint paint;
            paint.color(0xffffffff);
            DrawUniquePtr draw(
                renderContext->make<PushRetrofittedTrianglesGMDraw>(
                    renderContext,
                    &paint));
            bool success RIVE_MAYBE_UNUSED = renderContext->pushDraws(&draw, 1);
            assert(success);
        }
    }
};

GMREGISTER(return new RetrofittedCubicTrianglesGM;)

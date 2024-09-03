/*
 * Copyright 2023 Rive
 */

#include "gm.hpp"
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

class PushRetrofittedTrianglesGMDraw : public RiveRenderPathDraw
{
public:
    PushRetrofittedTrianglesGMDraw(const RiveRenderPaint* paint, gpu::InterlockMode interlockMode) :
        RiveRenderPathDraw(kFullscreenPixelBounds,
                           Mat2D(),
                           make_nonempty_placeholder_path(),
                           FillRule::nonZero,
                           paint,
                           Type::interiorTriangulationPath,
                           interlockMode)
    {
        m_resourceCounts.pathCount = 1;
        m_resourceCounts.contourCount = 1;
        m_resourceCounts.maxTessellatedSegmentCount = kNumTriangles;
        m_resourceCounts.outerCubicTessVertexCount =
            interlockMode != gpu::InterlockMode::depthStencil
                ? gpu::kOuterCurvePatchSegmentSpan * kNumTriangles * 2
                : gpu::kOuterCurvePatchSegmentSpan * kNumTriangles;
    }

    void onPushToRenderContext(RenderContext::LogicalFlush* flush) override
    {
        flush->pushContour({0, 0}, true, 0 /* gpu::kOuterCurvePatchSegmentSpan - 2 */);
        for (const auto& pts : kTris)
        {
            Vec2D tri[4] = {pts[0], pts[1], {0, 0}, pts[2]};
            flush->pushCubic(tri,
                             {0, 0},
                             RETROFITTED_TRIANGLE_CONTOUR_FLAG,
                             gpu::kOuterCurvePatchSegmentSpan - 1,
                             1,
                             1);
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
        gpu::RenderContext* renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext)
        {
            TestingWindow::Get()->beginFrame(0xffff0000, true);
        }
        else
        {
            TestingWindow::Get()->beginFrame(0xff000000, true);
            RiveRenderPaint paint;
            paint.color(0xffffffff);
            DrawUniquePtr draw(renderContext->make<PushRetrofittedTrianglesGMDraw>(
                &paint,
                renderContext->frameInterlockMode()));
            bool success RIVE_MAYBE_UNUSED = renderContext->pushDrawBatch(&draw, 1);
            assert(success);
        }
    }
};

GMREGISTER(return new RetrofittedCubicTrianglesGM;)

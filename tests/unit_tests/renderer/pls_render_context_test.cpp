/*
 * Copyright 2024 Rive
 */

#include "rive/renderer/rive_renderer.hpp"
#include "common/render_context_null.hpp"
#include <catch.hpp>

class RenderContextNULLTest : public RenderContextNULL
{
public:
    double m_secondsOverride = 7; // Arbitrary baseline of 7 seconds.
    double secondsNow() const override { return m_secondsOverride; }
};

class RenderContextTest : public rive::gpu::RenderContext
{
public:
    using RenderContext::ResourceAllocationCounts;
    RenderContextTest() :
        RenderContext(std::make_unique<RenderContextNULLTest>())
    {}
    RenderContextNULLTest* testingImpl()
    {
        return static_impl_cast<RenderContextNULLTest>();
    }

    ResourceAllocationCounts currentResourceAllocations() const
    {
        return m_currentResourceAllocations;
    }
    double lastResourceTrimTimeInSeconds() const
    {
        return m_lastResourceTrimTimeInSeconds;
    }
};

namespace rive::gpu
{
TEST_CASE("ResourceAllocationCounts", "RenderContext")
{
    RenderContextTest::ResourceAllocationCounts allocs;
    allocs.pathBufferCount = 1;
    allocs.contourBufferCount = 2;
    allocs.gradSpanBufferCount = 4;
    allocs.tessSpanBufferCount = 5;
    allocs.triangleVertexBufferCount = 6;
    allocs.imageDrawUniformBufferCount = 7;
    allocs.gradTextureHeight = 8;
    allocs.tessTextureHeight = 9;

    allocs = allocs.toVec() * 2;
    CHECK(allocs.pathBufferCount == 2);
    CHECK(allocs.contourBufferCount == 4);
    CHECK(allocs.gradSpanBufferCount == 8);
    CHECK(allocs.tessSpanBufferCount == 10);
    CHECK(allocs.triangleVertexBufferCount == 12);
    CHECK(allocs.imageDrawUniformBufferCount == 14);
    CHECK(allocs.gradTextureHeight == 16);
    CHECK(allocs.tessTextureHeight == 18);

    RenderContextTest::ResourceAllocationCounts testAllocs;
    testAllocs.pathBufferCount = 18;
    testAllocs.contourBufferCount = 16;
    testAllocs.gradSpanBufferCount = 12;
    testAllocs.tessSpanBufferCount = 10;
    testAllocs.triangleVertexBufferCount = 8;
    testAllocs.imageDrawUniformBufferCount = 6;
    testAllocs.gradTextureHeight = 4;
    testAllocs.tessTextureHeight = 2;

    allocs = simd::if_then_else(allocs.toVec() <= testAllocs.toVec(),
                                testAllocs.toVec(),
                                allocs.toVec() * size_t(5));
    CHECK(allocs.pathBufferCount == 18);
    CHECK(allocs.contourBufferCount == 16);
    CHECK(allocs.gradSpanBufferCount == 12);
    CHECK(allocs.tessSpanBufferCount == 10);
    CHECK(allocs.triangleVertexBufferCount == 12 * 5);
    CHECK(allocs.imageDrawUniformBufferCount == 14 * 5);
    CHECK(allocs.gradTextureHeight == 16 * 5);
    CHECK(allocs.tessTextureHeight == 18 * 5);

    allocs =
        simd::if_then_else(testAllocs.toVec() * size_t(2) <= allocs.toVec(),
                           allocs.toVec() / size_t(2),
                           allocs.toVec());
    CHECK(allocs.pathBufferCount == 18);
    CHECK(allocs.contourBufferCount == 16);
    CHECK(allocs.gradSpanBufferCount == 12);
    CHECK(allocs.tessSpanBufferCount == 10);
    CHECK(allocs.triangleVertexBufferCount == 6 * 5);
    CHECK(allocs.imageDrawUniformBufferCount == 7 * 5);
    CHECK(allocs.gradTextureHeight == 8 * 5);
    CHECK(allocs.tessTextureHeight == 9 * 5);
}

// Check that resources start getting trimmed after 5 seconds.
//
// (ResourceAllocationCounts proves that the SIMD handling of resources works,
// so here we only check path and contour counts, and assume the rest will
// follow suite.)
TEST_CASE("ResourceTriming", "RenderContext")
{
    RenderContextTest ctx;
    rive::RiveRenderer renderer(&ctx);

    CHECK(ctx.lastResourceTrimTimeInSeconds() ==
          ctx.testingImpl()->m_secondsOverride);

    CHECK(ctx.currentResourceAllocations().flushUniformBufferCount == 0);
    CHECK(ctx.currentResourceAllocations().imageDrawUniformBufferCount == 0);
    CHECK(ctx.currentResourceAllocations().pathBufferCount == 0);
    CHECK(ctx.currentResourceAllocations().paintBufferCount == 0);
    CHECK(ctx.currentResourceAllocations().paintAuxBufferCount == 0);
    CHECK(ctx.currentResourceAllocations().contourBufferCount == 0);
    CHECK(ctx.currentResourceAllocations().gradSpanBufferCount == 0);
    CHECK(ctx.currentResourceAllocations().tessSpanBufferCount == 0);
    CHECK(ctx.currentResourceAllocations().triangleVertexBufferCount == 0);
    CHECK(ctx.currentResourceAllocations().gradTextureHeight == 0);
    CHECK(ctx.currentResourceAllocations().tessTextureHeight == 0);

    auto twoContourPath = ctx.makeEmptyRenderPath();
    twoContourPath->addRect(0, 0, 100, 100);
    twoContourPath->addRect(20, 20, 80, 80);

    auto paint = ctx.makeRenderPaint();

    auto makeSimpleFrameDescriptor = []() {
        RenderContext::FrameDescriptor desc = {
            .renderTargetWidth = 200,
            .renderTargetHeight = 200,
        };
        return desc;
    };

    auto renderTarget = ctx.testingImpl()->makeRenderTarget(200, 200);

    ctx.beginFrame(makeSimpleFrameDescriptor());
    renderer.drawPath(twoContourPath.get(), paint.get());
    ctx.flush({.renderTarget = renderTarget.get()});

    size_t baselinePathCount = ctx.currentResourceAllocations().pathBufferCount;
    CHECK(baselinePathCount <= gpu::kPathBufferAlignmentInElements * 2);
    size_t baselineContourCount =
        ctx.currentResourceAllocations().contourBufferCount;
    CHECK(baselineContourCount <= gpu::kContourBufferAlignmentInElements * 2);

    // Check that resources grow to fit.
    auto lastCounts = ctx.currentResourceAllocations();
    for (int i = 10; i <= 1000; i *= 10)
    {
        ctx.beginFrame(makeSimpleFrameDescriptor());
        for (int j = 0; j < i; ++j)
        {
            renderer.drawPath(twoContourPath.get(), paint.get());
        }
        ctx.flush({.renderTarget = renderTarget.get()});

        CHECK(ctx.currentResourceAllocations().pathBufferCount >
              lastCounts.pathBufferCount);
        CHECK(ctx.currentResourceAllocations().contourBufferCount >
              lastCounts.contourBufferCount);
        lastCounts = ctx.currentResourceAllocations();
    }

    // Check that resources don't shrink before the time limit has passed.
    for (int i = 1000; i > 0; i /= 10)
    {
        ctx.beginFrame(makeSimpleFrameDescriptor());
        for (int j = 0; j < i; ++j)
        {
            renderer.drawPath(twoContourPath.get(), paint.get());
        }
        ctx.flush({.renderTarget = renderTarget.get()});

        CHECK(ctx.currentResourceAllocations().pathBufferCount ==
              lastCounts.pathBufferCount);
        CHECK(ctx.currentResourceAllocations().contourBufferCount ==
              lastCounts.contourBufferCount);
    }

    // Drawing nothing shouldn't shrink any resources either.
    ctx.beginFrame(makeSimpleFrameDescriptor());
    ctx.flush({.renderTarget = renderTarget.get()});
    CHECK(ctx.currentResourceAllocations().pathBufferCount ==
          lastCounts.pathBufferCount);
    CHECK(ctx.currentResourceAllocations().contourBufferCount ==
          lastCounts.contourBufferCount);

    // Pass the time limit and observe that allocations stay the same, since the
    // previous usage is still in m_maxRecentResourceRequirements.
    ctx.testingImpl()->m_secondsOverride += 5.0001;
    ctx.beginFrame(makeSimpleFrameDescriptor());
    renderer.drawPath(twoContourPath.get(), paint.get());
    ctx.flush({.renderTarget = renderTarget.get()});
    CHECK(ctx.currentResourceAllocations().pathBufferCount ==
          lastCounts.pathBufferCount);
    CHECK(ctx.currentResourceAllocations().contourBufferCount ==
          lastCounts.contourBufferCount);

    // Pass the time limit once more. This time resources should shrink since
    // m_maxRecentResourceRequirements only has the one path now.
    ctx.testingImpl()->m_secondsOverride += 5.0001;
    ctx.beginFrame(makeSimpleFrameDescriptor());
    renderer.drawPath(twoContourPath.get(), paint.get());
    ctx.flush({.renderTarget = renderTarget.get()});
    CHECK(ctx.currentResourceAllocations().pathBufferCount ==
          baselinePathCount);
    CHECK(ctx.currentResourceAllocations().contourBufferCount ==
          baselineContourCount);

    // Releasing resources should reset them all the way to zero.
    ctx.releaseResources();
    CHECK(ctx.currentResourceAllocations().flushUniformBufferCount == 0);
    CHECK(ctx.currentResourceAllocations().imageDrawUniformBufferCount == 0);
    CHECK(ctx.currentResourceAllocations().pathBufferCount == 0);
    CHECK(ctx.currentResourceAllocations().paintBufferCount == 0);
    CHECK(ctx.currentResourceAllocations().paintAuxBufferCount == 0);
    CHECK(ctx.currentResourceAllocations().contourBufferCount == 0);
    CHECK(ctx.currentResourceAllocations().gradSpanBufferCount == 0);
    CHECK(ctx.currentResourceAllocations().tessSpanBufferCount == 0);
    CHECK(ctx.currentResourceAllocations().triangleVertexBufferCount == 0);
    CHECK(ctx.currentResourceAllocations().gradTextureHeight == 0);
    CHECK(ctx.currentResourceAllocations().tessTextureHeight == 0);
}
} // namespace rive::gpu

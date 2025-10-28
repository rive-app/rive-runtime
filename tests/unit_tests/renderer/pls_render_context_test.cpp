/*
 * Copyright 2024 Rive
 */

#include "rive/renderer/rive_renderer.hpp"
#include "common/render_context_null.hpp"
#include <catch.hpp>

// Rather than spot-disabling this warning at call sites, for test code
//  we can just disable it for the whole file.
DISABLE_CLANG_SIMD_ABI_WARNING()

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

using ResourceAllocationCounts = RenderContextTest::ResourceAllocationCounts;

namespace rive::gpu
{
TEST_CASE("ResourceAllocationCounts", "RenderContext")
{
    ResourceAllocationCounts allocs;
    allocs.pathBufferCount = 1;
    allocs.contourBufferCount = 2;
    allocs.gradSpanBufferCount = 4;
    allocs.tessSpanBufferCount = 5;
    allocs.triangleVertexBufferCount = 6;
    allocs.imageDrawUniformBufferCount = 7;
    allocs.gradTextureHeight = 8;
    allocs.tessTextureHeight = 9;

    allocs = ResourceAllocationCounts::FromVec(allocs.toVec() * 2);
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

    allocs = ResourceAllocationCounts::FromVec(
        simd::if_then_else(allocs.toVec() <= testAllocs.toVec(),
                           testAllocs.toVec(),
                           allocs.toVec() * size_t(5)));
    CHECK(allocs.pathBufferCount == 18);
    CHECK(allocs.contourBufferCount == 16);
    CHECK(allocs.gradSpanBufferCount == 12);
    CHECK(allocs.tessSpanBufferCount == 10);
    CHECK(allocs.triangleVertexBufferCount == 12 * 5);
    CHECK(allocs.imageDrawUniformBufferCount == 14 * 5);
    CHECK(allocs.gradTextureHeight == 16 * 5);
    CHECK(allocs.tessTextureHeight == 18 * 5);

    allocs = ResourceAllocationCounts::FromVec(
        simd::if_then_else(testAllocs.toVec() * size_t(2) <= allocs.toVec(),
                           allocs.toVec() / size_t(2),
                           allocs.toVec()));
    CHECK(allocs.pathBufferCount == 18);
    CHECK(allocs.contourBufferCount == 16);
    CHECK(allocs.gradSpanBufferCount == 12);
    CHECK(allocs.tessSpanBufferCount == 10);
    CHECK(allocs.triangleVertexBufferCount == 6 * 5);
    CHECK(allocs.imageDrawUniformBufferCount == 7 * 5);
    CHECK(allocs.gradTextureHeight == 8 * 5);
    CHECK(allocs.tessTextureHeight == 9 * 5);
}

constexpr static double RESOURCE_EXPIRATION_TIME = 5.0001;

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

    CHECK(simd::all(ctx.currentResourceAllocations().toVec() == 0));

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
    ctx.testingImpl()->m_secondsOverride += RESOURCE_EXPIRATION_TIME;
    ctx.beginFrame(makeSimpleFrameDescriptor());
    renderer.drawPath(twoContourPath.get(), paint.get());
    ctx.flush({.renderTarget = renderTarget.get()});
    CHECK(ctx.currentResourceAllocations().pathBufferCount ==
          lastCounts.pathBufferCount);
    CHECK(ctx.currentResourceAllocations().contourBufferCount ==
          lastCounts.contourBufferCount);

    // Pass the time limit once more. This time resources should shrink since
    // m_maxRecentResourceRequirements only has the one path now.
    ctx.testingImpl()->m_secondsOverride += RESOURCE_EXPIRATION_TIME;
    ctx.beginFrame(makeSimpleFrameDescriptor());
    renderer.drawPath(twoContourPath.get(), paint.get());
    ctx.flush({.renderTarget = renderTarget.get()});
    CHECK(ctx.currentResourceAllocations().pathBufferCount ==
          baselinePathCount);
    CHECK(ctx.currentResourceAllocations().contourBufferCount ==
          baselineContourCount);

    // Releasing resources should reset them all the way to zero.
    ctx.releaseResources();
    CHECK(simd::all(ctx.currentResourceAllocations().toVec() == 0));
}

TEST_CASE("PLSResourceAllocation", "RenderContext")
{
    RenderContextTest ctx;
    rive::RiveRenderer renderer(&ctx);

    CHECK(ctx.lastResourceTrimTimeInSeconds() ==
          ctx.testingImpl()->m_secondsOverride);

    CHECK(simd::all(ctx.currentResourceAllocations().toVec() == 0));

    auto path = ctx.makeEmptyRenderPath();
    path->addRect(50, 50, 50, 50);

    auto paint = ctx.makeRenderPaint();

    CHECK(simd::all(ctx.currentResourceAllocations().toVec() == 0));

    auto doSimpleFrame = [&](RenderContext::FrameDescriptor desc) {
        ctx.beginFrame(desc);
        renderer.drawPath(path.get(), paint.get());
        auto renderTarget =
            ctx.testingImpl()->makeRenderTarget(desc.renderTargetWidth,
                                                desc.renderTargetHeight);
        ctx.flush({.renderTarget = renderTarget.get()});
    };

    // MSAA doesn't use PLS.
    doSimpleFrame({
        .renderTargetWidth = 100,
        .renderTargetHeight = 100,
        .msaaSampleCount = 4,
    });
    CHECK(ctx.currentResourceAllocations().plsTransientBackingWidth == 0);
    CHECK(ctx.currentResourceAllocations().plsTransientBackingHeight == 0);
    CHECK(ctx.currentResourceAllocations().plsTransientBackingPlaneCount == 0);
    CHECK(ctx.currentResourceAllocations().plsAtomicCoverageBackingWidth == 0);
    CHECK(ctx.currentResourceAllocations().plsAtomicCoverageBackingHeight == 0);

    // Check atomic.
    doSimpleFrame({
        .renderTargetWidth = 100,
        .renderTargetHeight = 101,
        .disableRasterOrdering = true,
    });
    // PLS backings are never over-allocated.
    CHECK(ctx.currentResourceAllocations().plsTransientBackingWidth == 100);
    CHECK(ctx.currentResourceAllocations().plsTransientBackingHeight == 101);
    CHECK(ctx.currentResourceAllocations().plsTransientBackingPlaneCount == 1);
    CHECK(ctx.currentResourceAllocations().plsAtomicCoverageBackingWidth ==
          100);
    CHECK(ctx.currentResourceAllocations().plsAtomicCoverageBackingHeight ==
          101);

    // Check raster ordering.
    doSimpleFrame({
        .renderTargetWidth = 101,
        .renderTargetHeight = 100,
    });
    // PLS backings are never over-allocated.
    CHECK(ctx.currentResourceAllocations().plsTransientBackingWidth == 101);
    CHECK(ctx.currentResourceAllocations().plsTransientBackingHeight == 101);
    CHECK(ctx.currentResourceAllocations().plsTransientBackingPlaneCount == 3);
    // The atomic backing isn't deallocated yet, but its size is not affected by
    // the backing for raster ordering.
    CHECK(ctx.currentResourceAllocations().plsAtomicCoverageBackingWidth ==
          100);
    CHECK(ctx.currentResourceAllocations().plsAtomicCoverageBackingHeight ==
          101);

    // Check raster ordering again after shrinking.
    for (size_t i = 0; i < 2; ++i)
    {
        ctx.testingImpl()->m_secondsOverride += RESOURCE_EXPIRATION_TIME;
        doSimpleFrame({
            .renderTargetWidth = 101,
            .renderTargetHeight = 100,
        });
    }
    CHECK(ctx.currentResourceAllocations().plsTransientBackingWidth == 101);
    // PLS backings are never over-allocated.
    CHECK(ctx.currentResourceAllocations().plsTransientBackingHeight == 100);
    CHECK(ctx.currentResourceAllocations().plsTransientBackingPlaneCount == 3);
    // The atomic backing should have been released now.
    CHECK(ctx.currentResourceAllocations().plsAtomicCoverageBackingWidth == 0);
    CHECK(ctx.currentResourceAllocations().plsAtomicCoverageBackingHeight == 0);

    // Check atomic shrinking.
    for (size_t i = 0; i < 2; ++i)
    {
        ctx.testingImpl()->m_secondsOverride += RESOURCE_EXPIRATION_TIME;
        doSimpleFrame({
            .renderTargetWidth = 99,
            .renderTargetHeight = 200,
            .disableRasterOrdering = true,
        });
    }
    // PLS backings are never over-allocated.
    CHECK(ctx.currentResourceAllocations().plsTransientBackingWidth == 99);
    CHECK(ctx.currentResourceAllocations().plsTransientBackingHeight == 200);
    // The backing planeCount should have shrunk since we've only been drawing
    // in atomic mode.
    CHECK(ctx.currentResourceAllocations().plsTransientBackingPlaneCount == 1);
    // The atomic backing should have shrunk now.
    CHECK(ctx.currentResourceAllocations().plsAtomicCoverageBackingWidth == 99);
    CHECK(ctx.currentResourceAllocations().plsAtomicCoverageBackingHeight ==
          200);

    // Check msaa shrinking.
    for (size_t i = 0; i < 2; ++i)
    {
        ctx.testingImpl()->m_secondsOverride += RESOURCE_EXPIRATION_TIME;
        doSimpleFrame({
            .renderTargetWidth = 1000,
            .renderTargetHeight = 1000,
            .msaaSampleCount = 4,
        });
    }
    // PLS backings are never over-allocated.
    CHECK(ctx.currentResourceAllocations().plsTransientBackingWidth == 0);
    CHECK(ctx.currentResourceAllocations().plsTransientBackingHeight == 0);
    CHECK(ctx.currentResourceAllocations().plsTransientBackingPlaneCount == 0);
    // The atomic backing should have been released now.
    CHECK(ctx.currentResourceAllocations().plsAtomicCoverageBackingWidth == 0);
    CHECK(ctx.currentResourceAllocations().plsAtomicCoverageBackingHeight == 0);

    // We may not need to allocate a full backing if we don't draw the full
    // render target (e.g, by preserving or "dontCare"), but for now we don't
    // worry about that. So preserve and dontCare should allocate full size
    // backings as well.
    doSimpleFrame({
        .renderTargetWidth = 999,
        .renderTargetHeight = 1234,
        .loadAction = gpu::LoadAction::preserveRenderTarget,
        .disableRasterOrdering = true,
    });
    CHECK(ctx.currentResourceAllocations().plsTransientBackingWidth == 999);
    CHECK(ctx.currentResourceAllocations().plsTransientBackingHeight == 1234);
    CHECK(ctx.currentResourceAllocations().plsTransientBackingPlaneCount == 1);
    CHECK(ctx.currentResourceAllocations().plsAtomicCoverageBackingWidth ==
          999);
    CHECK(ctx.currentResourceAllocations().plsAtomicCoverageBackingHeight ==
          1234);
    doSimpleFrame({
        .renderTargetWidth = 999,
        .renderTargetHeight = 1234,
        .loadAction = gpu::LoadAction::dontCare,
        .disableRasterOrdering = true,
    });
    CHECK(ctx.currentResourceAllocations().plsTransientBackingWidth == 999);
    CHECK(ctx.currentResourceAllocations().plsTransientBackingHeight == 1234);
    CHECK(ctx.currentResourceAllocations().plsTransientBackingPlaneCount == 1);
    CHECK(ctx.currentResourceAllocations().plsAtomicCoverageBackingWidth ==
          999);
    CHECK(ctx.currentResourceAllocations().plsAtomicCoverageBackingHeight ==
          1234);

    // Clearing the render target updates its full bounds, and sometimes PLS
    // implements the clear as well, so PLS always has to cover the full bounds.
    doSimpleFrame({
        .renderTargetWidth = 999,
        .renderTargetHeight = 1234,
        .loadAction = gpu::LoadAction::clear,
        .disableRasterOrdering = true,
    });
    CHECK(ctx.currentResourceAllocations().plsTransientBackingWidth == 999);
    CHECK(ctx.currentResourceAllocations().plsTransientBackingHeight == 1234);
    CHECK(ctx.currentResourceAllocations().plsTransientBackingPlaneCount == 1);
    CHECK(ctx.currentResourceAllocations().plsAtomicCoverageBackingWidth ==
          999);
    CHECK(ctx.currentResourceAllocations().plsAtomicCoverageBackingHeight ==
          1234);

    // Partial updates would eventually shrink the PLS backing if we ever don't
    // allocate the full render target size.
    renderer.save();
    renderer.scale(.5, 2);
    for (size_t i = 0; i < 2; ++i)
    {
        ctx.testingImpl()->m_secondsOverride += RESOURCE_EXPIRATION_TIME;
        doSimpleFrame({
            .renderTargetWidth = 1000,
            .renderTargetHeight = 1000,
            .loadAction = i == 0 ? gpu::LoadAction::preserveRenderTarget
                                 : gpu::LoadAction::dontCare,
            .disableRasterOrdering = true,
        });
    }
    CHECK(ctx.currentResourceAllocations().plsTransientBackingWidth == 1000);
    CHECK(ctx.currentResourceAllocations().plsTransientBackingHeight == 1000);
    CHECK(ctx.currentResourceAllocations().plsTransientBackingPlaneCount == 1);
    CHECK(ctx.currentResourceAllocations().plsAtomicCoverageBackingWidth ==
          1000);
    CHECK(ctx.currentResourceAllocations().plsAtomicCoverageBackingHeight ==
          1000);
    renderer.restore();

    ctx.releaseResources();
    CHECK(ctx.currentResourceAllocations().plsTransientBackingWidth == 0);
    CHECK(ctx.currentResourceAllocations().plsTransientBackingHeight == 0);
    CHECK(ctx.currentResourceAllocations().plsTransientBackingPlaneCount == 0);
    CHECK(ctx.currentResourceAllocations().plsAtomicCoverageBackingWidth == 0);
    CHECK(ctx.currentResourceAllocations().plsAtomicCoverageBackingHeight == 0);
}
} // namespace rive::gpu

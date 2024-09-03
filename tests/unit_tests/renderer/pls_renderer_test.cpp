/*
 * Copyright 2022 Rive
 */

#include "common/render_context_null.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "../src/rive_render_path.hpp"
#include <catch.hpp>

namespace rive::gpu
{
static RenderContext::FrameDescriptor s_frameDescriptor = {
    .renderTargetWidth = 100,
    .renderTargetHeight = 100,
};

// Ensures that clip contents get reused when we pop and push the same path(s).
TEST_CASE("clip-stack", "RiveRenderer")
{
    std::unique_ptr<RenderContext> renderContext = RenderContextNULL::MakeContext();
    auto renderTarget = renderContext->static_impl_cast<RenderContextNULL>()->makeRenderTarget(
        s_frameDescriptor.renderTargetWidth,
        s_frameDescriptor.renderTargetHeight);

    for (int i = 0; i < 3; ++i)
    {
        renderContext->beginFrame(s_frameDescriptor);

        auto pathA = renderContext->makeEmptyRenderPath();
        auto pathB = renderContext->makeEmptyRenderPath();
        auto pathC = renderContext->makeEmptyRenderPath();
        auto drawablePath = renderContext->makeEmptyRenderPath();
        pathA->cubicTo(1, 1, 1, 2, 2, 2);
        pathA->lineTo(3, 4);
        pathB->cubicTo(5, 5, 5, 6, 6, 6);
        pathB->lineTo(7, 8);
        pathC->lineTo(9, 10);
        pathC->cubicTo(11, 12, 13, 14, 15, 16);
        drawablePath->cubicTo(17, 17, 17, 18, 18, 18);
        drawablePath->lineTo(19, 20);

        auto paint = renderContext->makeRenderPaint();
        paint->color(0xffffffff);

        RiveRenderer renderer(renderContext.get());

        renderer.save();
        renderer.clipPath(pathA.get());
        renderer.drawPath(drawablePath.get(), paint.get());
        renderer.restore();
        uint32_t clipID = renderContext->getClipContentID();
        CHECK(clipID != 0);

        // Pushing the same clip sholdn't cause us to redraw the clipBuffer (clipID will stay the
        // same.)
        renderer.save();
        renderer.clipPath(pathA.get());
        renderer.drawPath(drawablePath.get(), paint.get());
        renderer.restore();
        CHECK(clipID == renderContext->getClipContentID());

        // We can't modify paths anymore once they've been pushed to the context.
#if 0
        // Modifying the path will cause the clip to be regenerated.
        renderer.save();
        pathA->lineTo(21, 22);
        renderer.clipPath(pathA.get());
        renderer.drawPath(drawablePath.get(), paint.get());
        renderer.restore();
        CHECK(clipID != renderContext->getClipContentID());
        clipID = renderContext->getClipContentID();
#endif

        // Pushing the same (modified) clip sholdn't cause us to redraw the clipBuffer, again.
        renderer.save();
        renderer.clipPath(pathA.get());
        renderer.drawPath(drawablePath.get(), paint.get());
        renderer.restore();
        CHECK(clipID == renderContext->getClipContentID());

        // Changing the view matrix will invalidate the clip.
        renderer.translate(.5f, 1.5f);
        renderer.save();
        renderer.clipPath(pathA.get());
        renderer.drawPath(drawablePath.get(), paint.get());
        renderer.restore();
        CHECK(clipID != renderContext->getClipContentID());
        clipID = renderContext->getClipContentID();

        // Now we should reuse the clip again on the new matrix.
        renderer.save();
        renderer.clipPath(pathA.get());
        renderer.drawPath(drawablePath.get(), paint.get());
        renderer.restore();
        CHECK(clipID == renderContext->getClipContentID());

        // Pushing and popping a nested clip will cause the clip to be regenerated.
        renderer.save();
        renderer.clipPath(pathA.get());
        renderer.drawPath(drawablePath.get(), paint.get());
        CHECK(clipID == renderContext->getClipContentID());
        renderer.clipPath(pathB.get());
        renderer.drawPath(drawablePath.get(), paint.get());
        CHECK(clipID != renderContext->getClipContentID());
        clipID = renderContext->getClipContentID();
        renderer.restore();

        // Pushing both back will reuse the clip.
        renderer.save();
        renderer.clipPath(pathA.get());
        renderer.clipPath(pathB.get());
        renderer.drawPath(drawablePath.get(), paint.get());
        renderer.restore();
        CHECK(clipID == renderContext->getClipContentID());

        // Pushing a new clip and not using it will also not affect the clip buffer.
        renderer.save();
        renderer.clipPath(pathA.get());
        renderer.clipPath(pathB.get());
        renderer.save();
        renderer.clipPath(pathC.get());
        renderer.restore();
        renderer.drawPath(drawablePath.get(), paint.get());
        CHECK(clipID == renderContext->getClipContentID());
        renderer.restore();

        // Now go three deep, just for fun.
        renderer.save();
        renderer.clipPath(pathA.get());
        renderer.save();
        renderer.clipPath(pathB.get());
        renderer.clipPath(pathC.get());
        renderer.drawPath(drawablePath.get(), paint.get());
        CHECK(clipID != renderContext->getClipContentID());
        clipID = renderContext->getClipContentID();
        renderer.restore();
        renderer.clipPath(pathB.get());
        renderer.clipPath(pathC.get());
        renderer.drawPath(drawablePath.get(), paint.get());
        CHECK(clipID == renderContext->getClipContentID());
        renderer.restore();

        // Adding them back in a different order will regenerate the clip.
        renderer.save();
        renderer.clipPath(pathA.get());
        renderer.clipPath(pathC.get());
        renderer.clipPath(pathB.get());
        renderer.drawPath(drawablePath.get(), paint.get());
        CHECK(clipID != renderContext->getClipContentID());
        clipID = renderContext->getClipContentID();
        renderer.restore();

        // And now the alternate order should stick.
        renderer.save();
        renderer.clipPath(pathA.get());
        renderer.clipPath(pathC.get());
        renderer.clipPath(pathB.get());
        renderer.drawPath(drawablePath.get(), paint.get());
        CHECK(clipID == renderContext->getClipContentID());
        renderer.restore();

        for (bool logicalFlush : {false, true})
        {
            if (logicalFlush)
            {
                renderContext->logicalFlush();
            }
            else
            {
                renderContext->flush({.renderTarget = renderTarget.get()});
                renderContext->beginFrame(s_frameDescriptor);
            }

            // The clip content ID gets reset after a flush.
            CHECK(renderContext->getClipContentID() == 0);

            // The clip gets cleared after a flush, so this should start over with a new clip ID.
            renderer.save();
            renderer.clipPath(pathA.get());
            renderer.clipPath(pathC.get());
            renderer.clipPath(pathB.get());
            renderer.drawPath(drawablePath.get(), paint.get());
            CHECK(clipID != renderContext->getClipContentID());
            CHECK(renderContext->getClipContentID() == 3);
            clipID = renderContext->getClipContentID();
            renderer.restore();

            // And now that we're in the new flush, doing it again will reuse the same clipID.
            renderer.save();
            renderer.clipPath(pathA.get());
            renderer.clipPath(pathC.get());
            renderer.clipPath(pathB.get());
            renderer.drawPath(drawablePath.get(), paint.get());
            renderer.restore();
            CHECK(clipID == renderContext->getClipContentID());

            // Draw another clip so the clipID isn't 1 anymore.
            renderer.save();
            renderer.clipPath(pathA.get());
            renderer.drawPath(drawablePath.get(), paint.get());
            renderer.restore();
            CHECK(clipID != renderContext->getClipContentID());
            clipID = renderContext->getClipContentID();
        }

        renderContext->flush({.renderTarget = renderTarget.get()});
    }
}

static RawPath& make_oval(const AABB& bounds)
{
    static RawPath path;
    path.rewind();
    path.addOval(bounds);
    return path;
};

// Ensures that a flush only invalidates the current clip once, and that we can start reusing it
// again after.
TEST_CASE("clip-flush-clip-clip", "RiveRenderer")
{
    std::unique_ptr<RenderContext> renderContext = RenderContextNULL::MakeContext();

    renderContext->beginFrame(s_frameDescriptor);

    auto pathA = renderContext->makeRenderPath(make_oval({0, 0, 1, 5}), FillRule::nonZero);
    auto pathB = renderContext->makeRenderPath(make_oval({0, 0, 2, 6}), FillRule::nonZero);
    auto pathC = renderContext->makeRenderPath(make_oval({0, 0, 3, 7}), FillRule::nonZero);
    auto pathD = renderContext->makeRenderPath(make_oval({0, 0, 4, 8}), FillRule::nonZero);
    auto drawablePath = renderContext->makeRenderPath(make_oval({0, 0, 9, 10}), FillRule::nonZero);

    auto paint = renderContext->makeRenderPaint();
    paint->color(0xffffffff);

    RiveRenderer renderer(renderContext.get());

    renderer.save();
    renderer.clipPath(pathD.get());
    renderer.drawPath(drawablePath.get(), paint.get());
    uint32_t clipID = renderContext->getClipContentID();
    CHECK(clipID != 0);
    renderer.restore();

    renderer.clipPath(pathC.get());
    renderer.clipPath(pathB.get());
    renderer.clipPath(pathA.get());
    renderer.drawPath(drawablePath.get(), paint.get());
    CHECK(renderContext->getClipContentID() != clipID);
    clipID = renderContext->getClipContentID();

    // Flushing should invalidate the clip.
    renderContext->logicalFlush();
    renderer.drawPath(drawablePath.get(), paint.get());
    CHECK(renderContext->getClipContentID() != clipID);
    clipID = renderContext->getClipContentID();

    // The clip should now get reused again for subsequent draws.
    for (size_t i = 0; i < 3; ++i)
    {
        renderer.drawPath(drawablePath.get(), paint.get());
        CHECK(renderContext->getClipContentID() == clipID);
    }

    auto renderTarget = renderContext->static_impl_cast<RenderContextNULL>()->makeRenderTarget(
        s_frameDescriptor.renderTargetWidth,
        s_frameDescriptor.renderTargetHeight);
    renderContext->flush({.renderTarget = renderTarget.get()});
}

static RawPath& make_cusp(const IAABB& rect)
{
    static RawPath path;
    path.rewind();
    path.moveTo(rect.left, rect.top);
    path.lineTo(rect.right, rect.bottom);
    path.lineTo(rect.right, rect.top);
    path.lineTo(rect.left, rect.bottom);
    return path;
}

// Check that clip readBounds and contentBounds get tracked properly.
TEST_CASE("clip-content-read-bounds", "RiveRenderer")
{
    std::unique_ptr<RenderContext> renderContext = RenderContextNULL::MakeContext();

    auto renderTarget = renderContext->static_impl_cast<RenderContextNULL>()->makeRenderTarget(
        s_frameDescriptor.renderTargetWidth,
        s_frameDescriptor.renderTargetHeight);
    RenderContext::FrameDescriptor frameDescriptor = s_frameDescriptor;
    frameDescriptor.msaaSampleCount = 4;
    renderContext->beginFrame(frameDescriptor);

    auto cuspA = static_rcp_cast<RiveRenderPath>(
        renderContext->makeRenderPath(make_cusp({0, 0, 1, 5}), FillRule::nonZero));
    auto cuspB = static_rcp_cast<RiveRenderPath>(
        renderContext->makeRenderPath(make_cusp({-1, -1, 1, 1}), FillRule::nonZero));
    auto cuspC = static_rcp_cast<RiveRenderPath>(
        renderContext->makeRenderPath(make_cusp({0, 0, 5, 1}), FillRule::nonZero));

    auto paint = renderContext->makeRenderPaint();
    paint->color(0xffffffff);

    // Clip read bounds are affected by the view matrix.
    Mat2D xforms[] = {Mat2D(),
                      Mat2D::fromTranslate(3, 1),
                      Mat2D::fromScale(3, 2),
                      Mat2D::fromTranslate(-1, 3).scale({2, 3})};

    RiveRenderer renderer(renderContext.get());
    for (const Mat2D& m : xforms)
    {
        renderer.save();
        renderer.clipPath(cuspA.get());
        renderer.transform(m);
        renderer.drawPath(cuspB.get(), paint.get());

        uint32_t clipAID = renderContext->getClipContentID();
        REQUIRE(clipAID != 0);
        // clipA is not transformed.
        auto clipAContentBounds = cuspA->getBounds().roundOut();
        CHECK(renderContext->getClipContentBounds(clipAID) == clipAContentBounds);
        CHECK(renderContext->getClipReadBounds(clipAID) ==
              m.mapBoundingBox({-1, -1, 1, 1}).roundOut());

        renderer.clipPath(cuspC.get());
        renderer.drawPath(cuspB.get(), paint.get());
        uint32_t clipCID = renderContext->getClipContentID();
        REQUIRE(clipCID != 0);
        REQUIRE(clipCID != clipAID);
        auto clipCContentBounds = m.mapBoundingBox(cuspC->getBounds().roundOut()).roundOut();
        CHECK(renderContext->getClipContentBounds(clipCID) == clipCContentBounds);
        CHECK(renderContext->getClipReadBounds(clipCID) ==
              m.mapBoundingBox({-1, -1, 1, 1}).roundOut());

        // clipAID read bounds should have expanded from the nested clipping.
        CHECK(renderContext->getClipContentBounds(clipAID) == clipAContentBounds);
        CHECK(renderContext->getClipReadBounds(clipAID) ==
              m.mapBoundingBox({-1, -1, 5, 1}).roundOut());

        // Each nested clip is read only by the one directly below it.
        auto cusp6 = static_rcp_cast<RiveRenderPath>(
            renderContext->makeRenderPath(make_cusp({0, 0, 1, 6}), FillRule::nonZero));
        auto cusp7 = static_rcp_cast<RiveRenderPath>(
            renderContext->makeRenderPath(make_cusp({0, 0, 1, 7}), FillRule::nonZero));
        auto cusp8 = static_rcp_cast<RiveRenderPath>(
            renderContext->makeRenderPath(make_cusp({0, 0, 1, 8}), FillRule::nonZero));
        auto cusp9 = static_rcp_cast<RiveRenderPath>(
            renderContext->makeRenderPath(make_cusp({0, 0, 1, 9}), FillRule::nonZero));

        renderer.clipPath(cusp9.get());
        renderer.drawPath(cuspB.get(), paint.get());
        uint32_t clip9ID = renderContext->getClipContentID();

        renderer.clipPath(cusp8.get());
        renderer.drawPath(cuspB.get(), paint.get());
        uint32_t clip8ID = renderContext->getClipContentID();

        renderer.save();
        renderer.clipPath(cusp7.get());
        renderer.drawPath(cuspB.get(), paint.get());
        uint32_t clip7ID = renderContext->getClipContentID();

        renderer.clipPath(cusp6.get());
        renderer.drawPath(cuspC.get(), paint.get());
        uint32_t clip6ID = renderContext->getClipContentID();

        // clipA bounds should not have been affected by deeper nested clips.
        CHECK(renderContext->getClipContentBounds(clipAID) == clipAContentBounds);
        CHECK(renderContext->getClipReadBounds(clipAID) ==
              m.mapBoundingBox({-1, -1, 5, 1}).roundOut());

        // Each nested clip is read only by the one directly below it. Outer clips are not read by
        // the draw either.
        CHECK(renderContext->getClipContentBounds(clipCID) == clipCContentBounds);
        CHECK(renderContext->getClipReadBounds(clipCID) ==
              m.mapBoundingBox({-1, -1, 1, 9}).roundOut());
        auto clip9ContentBounds = m.mapBoundingBox(cusp9->getBounds().roundOut()).roundOut();
        CHECK(renderContext->getClipContentBounds(clip9ID) == clip9ContentBounds);
        CHECK(renderContext->getClipReadBounds(clip9ID) ==
              m.mapBoundingBox({-1, -1, 1, 8}).roundOut());
        auto clip8ContentBounds = m.mapBoundingBox(cusp8->getBounds().roundOut()).roundOut();
        CHECK(renderContext->getClipContentBounds(clip8ID) == clip8ContentBounds);
        CHECK(renderContext->getClipReadBounds(clip8ID) ==
              m.mapBoundingBox({-1, -1, 1, 7}).roundOut());
        auto clip7ContentBounds = m.mapBoundingBox(cusp7->getBounds().roundOut()).roundOut();
        CHECK(renderContext->getClipContentBounds(clip7ID) == clip7ContentBounds);
        CHECK(renderContext->getClipReadBounds(clip7ID) ==
              m.mapBoundingBox({-1, -1, 1, 6}).roundOut());
        auto clip6ContentBounds = m.mapBoundingBox(cusp6->getBounds().roundOut()).roundOut();
        CHECK(renderContext->getClipContentBounds(clip6ID) == clip6ContentBounds);
        CHECK(renderContext->getClipReadBounds(clip6ID) ==
              m.mapBoundingBox({0, 0, 5, 1}).roundOut());

        // Pop back and do some more reading from clip8.
        renderer.restore();
        renderer.drawPath(cuspC.get(), paint.get());
        uint32_t secondClip8ID = renderContext->getClipContentID();

        // Since clip8 got obliterated and redrawn, this next read shouldn't affect it.
        CHECK(secondClip8ID != clip8ID);
        CHECK(renderContext->getClipContentBounds(clip8ID) == clip8ContentBounds);
        CHECK(renderContext->getClipReadBounds(clip8ID) ==
              m.mapBoundingBox({-1, -1, 1, 7}).roundOut());
        // ... But should affect the clip currently in the clip buffer.
        CHECK(renderContext->getClipContentBounds(secondClip8ID) == clip8ContentBounds);
        CHECK(renderContext->getClipReadBounds(secondClip8ID) ==
              m.mapBoundingBox({0, 0, 5, 1}).roundOut());

        renderer.clipPath(cuspB.get());
        renderer.drawPath(cuspC.get(), paint.get());
        CHECK(renderContext->getClipContentBounds(clip8ID) == clip8ContentBounds);
        CHECK(renderContext->getClipReadBounds(clip8ID) ==
              m.mapBoundingBox({-1, -1, 1, 7}).roundOut());
        CHECK(renderContext->getClipContentBounds(secondClip8ID) == clip8ContentBounds);
        CHECK(renderContext->getClipReadBounds(secondClip8ID) ==
              m.mapBoundingBox({-1, -1, 5, 1}).roundOut());
        uint32_t clipBID = renderContext->getClipContentID();
        auto clipBContentBounds = m.mapBoundingBox(cuspB->getBounds().roundOut()).roundOut();
        CHECK(renderContext->getClipContentBounds(clipBID) == clipBContentBounds);
        CHECK(renderContext->getClipReadBounds(clipBID) ==
              m.mapBoundingBox({0, 0, 5, 1}).roundOut());

        renderer.restore();

        // Clip content bounds should never change.
        CHECK(renderContext->getClipContentBounds(clipAID) == clipAContentBounds);
        CHECK(renderContext->getClipContentBounds(clipCID) == clipCContentBounds);
        CHECK(renderContext->getClipContentBounds(clip9ID) == clip9ContentBounds);
        CHECK(renderContext->getClipContentBounds(clip8ID) == clip8ContentBounds);
        CHECK(renderContext->getClipContentBounds(clip7ID) == clip7ContentBounds);
        CHECK(renderContext->getClipContentBounds(clip6ID) == clip6ContentBounds);
        CHECK(renderContext->getClipContentBounds(secondClip8ID) == clip8ContentBounds);
        CHECK(renderContext->getClipContentBounds(clipBID) == clipBContentBounds);
    }

    renderContext->flush({.renderTarget = renderTarget.get()});
}

static RawPath& make_rect(const AABB& rect)
{
    static RawPath path;
    path.rewind();
    path.addRect(rect);
    return path;
};

#define CHECK_AABB_NEARLY_EQUAL(AABB, L, T, R, B)                                                  \
    {                                                                                              \
        CHECK(AABB.left() == Approx(L));                                                           \
        CHECK(AABB.top() == Approx(T));                                                            \
        CHECK(AABB.right() == Approx(R));                                                          \
        CHECK(AABB.bottom() == Approx(B));                                                         \
    }

// Ensures that rectangular clips are handled as "clipRects" where possible, instead of going to the
// clip buffer.
TEST_CASE("clip-rects", "RiveRenderer")
{
    std::unique_ptr<RenderContext> renderContext = RenderContextNULL::MakeContext();

    renderContext->beginFrame(s_frameDescriptor);

    auto rectA = renderContext->makeRenderPath(make_rect({0, 0, 1, 5}), FillRule::nonZero);
    auto rectB = renderContext->makeRenderPath(make_rect({1, -1, 2, 4}), FillRule::nonZero);
    auto rectC = renderContext->makeRenderPath(make_rect({0, 0, 3, 6}), FillRule::nonZero);
    auto ovalA = renderContext->makeRenderPath(make_oval({0, 0, 1, 4}), FillRule::nonZero);
    auto ovalB = renderContext->makeRenderPath(make_oval({0, 0, 2, 5}), FillRule::nonZero);
    auto ovalC = renderContext->makeRenderPath(make_oval({0, 0, 3, 6}), FillRule::nonZero);
    auto drawablePath = renderContext->makeRenderPath(make_oval({0, 0, 7, 8}), FillRule::nonZero);

    auto paint = renderContext->makeRenderPaint();
    paint->color(0xffffffff);

    RiveRenderer renderer(renderContext.get());

    // A single rect is always handled as a clipRect.
    renderer.save();
    renderer.clipPath(rectA.get());
    CHECK(renderContext->getClipContentID() == 0);
    renderer.drawPath(drawablePath.get(), paint.get());
    CHECK(renderContext->getClipContentID() == 0);
    renderer.restore();

    renderer.save();

    // The nested rect is always handled as a clipRect, even if it's nested.
    renderer.clipPath(ovalA.get());
    renderer.drawPath(drawablePath.get(), paint.get());
    uint32_t clipID = renderContext->getClipContentID();
    CHECK(clipID != 0);
    CHECK(!renderer.hasClipRect());
    renderer.clipPath(rectA.get());
    renderer.drawPath(drawablePath.get(), paint.get());
    CHECK(renderContext->getClipContentID() == clipID);
    CHECK(renderer.hasClipRect());
    CHECK_AABB_NEARLY_EQUAL(renderer.getClipRect(), 0, 0, 1, 5);
    CHECK(renderer.getClipRectMatrix() == Mat2D());

    // Doubly nested rectangles get combined into a single clipRect.
    renderer.clipPath(ovalB.get());
    renderer.drawPath(drawablePath.get(), paint.get());
    CHECK(renderContext->getClipContentID() != clipID);
    clipID = renderContext->getClipContentID();
    renderer.clipPath(rectB.get());
    renderer.drawPath(drawablePath.get(), paint.get());
    CHECK(renderContext->getClipContentID() == clipID);
    CHECK(renderer.hasClipRect());
    CHECK_AABB_NEARLY_EQUAL(renderer.getClipRect(), 1, 0, 1, 4);
    CHECK(renderer.getClipRectMatrix() == Mat2D());

    // Nested rectangles don't get combined if their matrices are incompatible.
    renderer.save();
    renderer.rotate(1);
    renderer.clipPath(ovalA.get());
    renderer.drawPath(drawablePath.get(), paint.get());
    CHECK(renderContext->getClipContentID() != clipID);
    clipID = renderContext->getClipContentID();
    renderer.clipPath(rectB.get());
    renderer.drawPath(drawablePath.get(), paint.get());
    CHECK(renderContext->getClipContentID() != clipID);
    clipID = renderContext->getClipContentID();
    renderer.restore();
    CHECK(renderer.hasClipRect());
    CHECK_AABB_NEARLY_EQUAL(renderer.getClipRect(), 1, 0, 1, 4);
    CHECK(renderer.getClipRectMatrix() == Mat2D());

    // Nested rectangles DO get combined if their matrices ARE compatible.
    renderer.save();
    renderer.scale(1, 2);
    renderer.translate(3, 4);
    renderer.clipPath(ovalC.get());
    renderer.drawPath(drawablePath.get(), paint.get());
    CHECK(renderContext->getClipContentID() != clipID);
    clipID = renderContext->getClipContentID();
    renderer.clipPath(rectC.get());
    renderer.drawPath(drawablePath.get(), paint.get());
    CHECK(renderContext->getClipContentID() == clipID);
    CHECK(renderer.hasClipRect());
    CHECK_AABB_NEARLY_EQUAL(renderer.getClipRect(), 3, 8, 1, 4);
    CHECK(renderer.getClipRectMatrix() == Mat2D());
    renderer.restore();

    renderer.restore();

    renderContext->logicalFlush();

    renderer.save();

    // Scaling and transforming within the same baseline space should still allow for clipRects to
    // be combined.
    Mat2D m = Mat2D::fromRotation(1) * Mat2D{.3f, .3f, .9f, -.7f, .4f, .2f};
    renderer.transform(m);
    renderer.clipPath(rectA.get());
    renderer.drawPath(drawablePath.get(), paint.get());
    CHECK(renderContext->getClipContentID() == 0);
    CHECK(renderer.hasClipRect());
    CHECK(renderer.getClipRect() == AABB{0, 0, 1, 5});
    CHECK(renderer.getClipRectMatrix() == m);
    renderer.clipPath(rectB.get());
    renderer.drawPath(drawablePath.get(), paint.get());
    CHECK(renderContext->getClipContentID() == 0);
    renderer.translate(.9f, -.1f);
    renderer.clipPath(rectB.get());
    renderer.drawPath(drawablePath.get(), paint.get());
    CHECK(renderContext->getClipContentID() == 0);
    CHECK(renderer.hasClipRect());
    CHECK_AABB_NEARLY_EQUAL(renderer.getClipRect(), 1.9f, 0, 1, 3.9f);
    CHECK(renderer.getClipRectMatrix() == m);
    renderer.scale(10, 2);
    renderer.clipPath(rectB.get());
    renderer.drawPath(drawablePath.get(), paint.get());
    CHECK(renderContext->getClipContentID() == 0);

    // Even 90 degree rotations work!
    renderer.rotate(math::PI / 2);
    renderer.clipPath(rectB.get());
    renderer.drawPath(drawablePath.get(), paint.get());
    CHECK(renderer.hasClipRect());
    CHECK_AABB_NEARLY_EQUAL(renderer.getClipRect(), 10.9f, 1.9f, 1, 3.9f);
    CHECK(renderer.getClipRectMatrix() == m);
    CHECK(renderContext->getClipContentID() == 0);
    renderer.rotate(math::PI);
    renderer.translate(-10, 0);
    renderer.clipPath(rectB.get());
    renderer.drawPath(drawablePath.get(), paint.get());
    CHECK(renderer.hasClipRect());
    CHECK_AABB_NEARLY_EQUAL(renderer.getClipRect(), 10.9f, 15.9f, 1, 3.9f);
    CHECK(renderer.getClipRectMatrix() == m);
    CHECK(renderContext->getClipContentID() == 0);
    renderer.rotate(-math::PI / 2);
    renderer.clipPath(rectB.get());
    renderer.drawPath(drawablePath.get(), paint.get());
    CHECK(renderContext->getClipContentID() == 0);
    CHECK(renderer.hasClipRect());
    CHECK_AABB_NEARLY_EQUAL(renderer.getClipRect(), 10.9f, 15.9f, -9.1f, 3.9f);
    CHECK(renderer.getClipRectMatrix() == m);

    // And flips!
    renderer.scale(1, -1);
    renderer.clipPath(rectB.get());
    renderer.drawPath(drawablePath.get(), paint.get());
    CHECK(renderContext->getClipContentID() == 0);
    CHECK(renderer.hasClipRect());
    CHECK_AABB_NEARLY_EQUAL(renderer.getClipRect(), 10.9f, 17.9f, -9.1f, 3.9f);
    CHECK(renderer.getClipRectMatrix() == m);
    renderer.scale(-1, -1);
    renderer.translate(2, 0);
    renderer.clipPath(rectB.get());
    renderer.drawPath(drawablePath.get(), paint.get());
    CHECK(renderContext->getClipContentID() == 0);
    CHECK(renderer.hasClipRect());
    CHECK_AABB_NEARLY_EQUAL(renderer.getClipRect(), 30.9f, 17.9f, -9.1f, 3.9f);
    CHECK(renderer.getClipRectMatrix() == m);

    // ... but once the matrices become incompatible, we can't intersect with the clipRect anymore.
    renderer.transform(Mat2D{1, .1f, 0, 1, 0, 0});
    renderer.clipPath(rectB.get());
    renderer.drawPath(drawablePath.get(), paint.get());
    CHECK(renderContext->getClipContentID() != 0);
    CHECK_AABB_NEARLY_EQUAL(renderer.getClipRect(), 30.9f, 17.9f, -9.1f, 3.9f);
    CHECK(renderer.getClipRectMatrix() == m);

    renderer.restore();

    auto renderTarget = renderContext->static_impl_cast<RenderContextNULL>()->makeRenderTarget(
        s_frameDescriptor.renderTargetWidth,
        s_frameDescriptor.renderTargetHeight);
    renderContext->flush({.renderTarget = renderTarget.get()});
}

// This test passes by not triggering assertions within RiveRenderer and RenderContext due to
// drawing empty paths.
TEST_CASE("empty-paths", "RiveRenderer")
{
    std::unique_ptr<RenderContext> renderContext = RenderContextNULL::MakeContext();
    renderContext->beginFrame(s_frameDescriptor);

    RiveRenderer renderer(renderContext.get());

    // Draw an empty path.
    auto paint = renderContext->makeRenderPaint();
    auto path = renderContext->makeEmptyRenderPath();
    renderer.drawPath(path.get(), paint.get());

    // Draw another empty path.
    path = renderContext->makeEmptyRenderPath();
    path->moveTo(1, 1);
    path->moveTo(2, 2);
    path->lineTo(2, 2);
    renderer.drawPath(path.get(), paint.get());

    // Draw a path with tons of vertices that definitely trigger a first-draw realloc.
    path = renderContext->makeEmptyRenderPath();
    auto rand100 = []() {
        return static_cast<float>(rand()) * 100.f / static_cast<float>(RAND_MAX);
    };
    for (size_t i = 0; i < 100000; ++i)
    {
        path->cubicTo(rand100(), rand100(), rand100(), rand100(), rand100(), rand100());
    }
    renderer.drawPath(path.get(), paint.get());

    // Draw one more empty path.
    renderer.drawPath(renderContext->makeEmptyRenderPath().get(), paint.get());

    auto renderTarget = renderContext->static_impl_cast<RenderContextNULL>()->makeRenderTarget(
        s_frameDescriptor.renderTargetWidth,
        s_frameDescriptor.renderTargetHeight);
    renderContext->flush({.renderTarget = renderTarget.get()});
}

// This test passes by not triggering assertions within RiveRenderer and RenderContext due to
// drawing empty paths composed of nothing but moves.
TEST_CASE("empty-paths2", "RiveRenderer")
{
    std::unique_ptr<RenderContext> renderContext = RenderContextNULL::MakeContext();
    renderContext->beginFrame(s_frameDescriptor);

    RiveRenderer renderer(renderContext.get());

    auto paint = renderContext->makeRenderPaint();

    // Draw path composed of nothing but moves.
    auto path = renderContext->makeEmptyRenderPath();
    path->moveTo(0, 0);
    path->moveTo(1, 2);
    path->moveTo(3, 4);
    path->moveTo(5, 6);
    renderer.drawPath(path.get(), paint.get());

    // Draw a large path (that triggers the triangulator) composed of nothing but moves.
    path = renderContext->makeEmptyRenderPath();
    path->moveTo(0, 0);
    path->moveTo(1000, 2000);
    path->moveTo(3000, 4000);
    path->moveTo(5000, 6000);
    path->moveTo(7000, 8000);
    renderer.drawPath(path.get(), paint.get());

    auto renderTarget = renderContext->static_impl_cast<RenderContextNULL>()->makeRenderTarget(
        s_frameDescriptor.renderTargetWidth,
        s_frameDescriptor.renderTargetHeight);
    renderContext->flush({.renderTarget = renderTarget.get()});
}

TEST_CASE("IsAABB", "RiveRenderer")
{
    AABB rect;
    for (bool doClose : {true, false})
    {
        RawPath path;
        if (doClose)
        {
            path.addRect(AABB{0, 0, 10, 10}, PathDirection::ccw);
        }
        else
        {
            path.lineTo(10, 0);
            path.lineTo(10, 10);
            path.lineTo(0, 10);
        }
        CHECK(RiveRenderer::IsAABB(path, &rect));
        CHECK(rect == AABB{0, 0, 10, 10});

        path.reset();
        if (doClose)
        {
            path.addRect(AABB{1, 2, -1, 5}, PathDirection::cw);
        }
        else
        {
            path.moveTo(1, 2);
            path.lineTo(-1, 2);
            path.lineTo(-1, 5);
            path.lineTo(1, 5);
        }
        CHECK(RiveRenderer::IsAABB(path, &rect));
        CHECK(rect == AABB{-1, 2, 1, 5});

        path.reset();
        if (doClose)
        {
            path.addRect(AABB{0, 0, 10, 10}, PathDirection::ccw);
            path.addRect(AABB{1, 2, -1, 5}, PathDirection::ccw);
        }
        else
        {
            path.lineTo(10, 0);
            path.lineTo(10, 10);
            path.lineTo(0, 10);
            path.lineTo(0, 0);
            path.lineTo(10, 0);
        }
        CHECK(!RiveRenderer::IsAABB(path, &rect));
    }

    RawPath path;
    path.moveTo(1, 0);
    CHECK(!RiveRenderer::IsAABB(path, &rect));
    path.lineTo(0, 0);
    CHECK(!RiveRenderer::IsAABB(path, &rect));
    path.lineTo(0, 1);
    CHECK(!RiveRenderer::IsAABB(path, &rect));
    path.lineTo(1, 1);
    CHECK(RiveRenderer::IsAABB(path, &rect));
    path.close();
    CHECK(RiveRenderer::IsAABB(path, &rect));
    CHECK(rect == AABB{0, 0, 1, 1});

    path.reset();
    path.moveTo(0, 0);
    path.lineTo(1, 0);
    path.lineTo(1, 1);
    path.lineTo(0, 1.1f);
    path.close();
    CHECK(!RiveRenderer::IsAABB(path, &rect));

    // Accept additional verbs and points after the AABB as long as every point after is equal to
    // p0.
    path.reset();
    path.moveTo(0, 0);
    path.lineTo(1, 0);
    path.lineTo(1, 1);
    path.lineTo(0, 1);
    CHECK(RiveRenderer::IsAABB(path, &rect));
    CHECK(rect == AABB{0, 0, 1, 1});
    path.close();
    CHECK(RiveRenderer::IsAABB(path, &rect));
    CHECK(rect == AABB{0, 0, 1, 1});
    path.reset();
    path.moveTo(1, 1);
    path.lineTo(0, 1);
    path.lineTo(0, 0);
    path.lineTo(1, 0);
    CHECK(RiveRenderer::IsAABB(path, &rect));
    CHECK(rect == AABB{0, 0, 1, 1});
    path.lineTo(1, 1);
    CHECK(RiveRenderer::IsAABB(path, &rect));
    CHECK(rect == AABB{0, 0, 1, 1});
    path.close();
    CHECK(RiveRenderer::IsAABB(path, &rect));
    CHECK(rect == AABB{0, 0, 1, 1});
    path.close();
    CHECK(RiveRenderer::IsAABB(path, &rect));
    CHECK(rect == AABB{0, 0, 1, 1});
    path.cubicTo(1, 1, 1, 1, 1, 1);
    CHECK(RiveRenderer::IsAABB(path, &rect));
    CHECK(rect == AABB{0, 0, 1, 1});
    path.quadTo(1, 1, 1, 1);
    CHECK(RiveRenderer::IsAABB(path, &rect));
    CHECK(rect == AABB{0, 0, 1, 1});
}

// Check that paths with NaN vertices don't crash.
TEST_CASE("nan-render-path", "RiveRenderer")
{
    std::unique_ptr<RenderContext> renderContext = RenderContextNULL::MakeContext();
    RiveRenderer renderer(renderContext.get());
    auto paint = renderContext->makeRenderPaint();

    auto finitePath = renderContext->makeEmptyRenderPath();
    finitePath->moveTo(1, 2);
    finitePath->lineTo(3, 4);
    finitePath->cubicTo(5, 6, 7, 8, 9, 10);
    finitePath->cubicTo(11, 12, 13, 14, 15, 16);
    finitePath->lineTo(17, 18);

    auto nan = std::numeric_limits<float>::quiet_NaN();
    auto nanPath = renderContext->makeEmptyRenderPath();
    nanPath->moveTo(nan, nan);
    nanPath->lineTo(nan, nan);
    nanPath->cubicTo(nan, nan, nan, nan, nan, nan);
    nanPath->cubicTo(nan, nan, nan, nan, nan, nan);
    nanPath->lineTo(nan, nan);

    renderContext->beginFrame(s_frameDescriptor);

    renderer.drawPath(nanPath.get(), paint.get());

    renderer.save();
    renderer.clipPath(nanPath.get());
    renderer.drawPath(finitePath.get(), paint.get());
    renderer.restore();

    renderer.save();
    renderer.transform({nan, nan, nan, nan, nan, nan});
    renderer.drawPath(finitePath.get(), paint.get());
    renderer.clipPath(finitePath.get());
    renderer.drawPath(nanPath.get(), paint.get());
    renderer.restore();

    CHECK(renderContext->getClipContentID() == 0);

    renderer.save();
    renderer.clipPath(nanPath.get());
    renderer.drawPath(finitePath.get(), paint.get());
    // NaN paths cause an immediatedly empty clip stack that just discards elements.
    CHECK(renderContext->getClipContentID() == 0);
    renderer.clipPath(finitePath.get());
    renderer.drawPath(finitePath.get(), paint.get());
    CHECK(renderContext->getClipContentID() == 0);
    renderer.restore();

    renderer.save();
    renderer.clipPath(finitePath.get());
    renderer.drawPath(finitePath.get(), paint.get());
    // Non-NaN paths update the clip content ID.
    CHECK(renderContext->getClipContentID() != 0);
    renderer.restore();

    // move(nan, nan), close() found a crash.
    auto moveNaN = renderContext->makeEmptyRenderPath();
    moveNaN->moveTo(nan, nan);
    moveNaN->close();
    renderer.drawPath(moveNaN.get(), paint.get());

    auto renderTarget = renderContext->static_impl_cast<RenderContextNULL>()->makeRenderTarget(
        s_frameDescriptor.renderTargetWidth,
        s_frameDescriptor.renderTargetHeight);
    renderContext->flush({.renderTarget = renderTarget.get()});
}

// Check that edge cases in stroke thickness don't crash or assert.
TEST_CASE("stroke-thickness", "RiveRenderer")
{
    std::unique_ptr<RenderContext> renderContext = RenderContextNULL::MakeContext();

    renderContext->beginFrame(s_frameDescriptor);

    auto path = renderContext->makeEmptyRenderPath();
    path->lineTo(100, 100);
    path->lineTo(100, 0);

    auto paint = renderContext->makeRenderPaint();
    paint->style(RenderPaintStyle::stroke);

    RiveRenderer renderer(renderContext.get());

    // Check a stroke thickness that truncates to 0 when converted to a radius
    // (i.e., when multiplied by .5).
    paint->thickness(math::bit_cast<float>(1u));
    renderer.drawPath(path.get(), paint.get());

    // Check NaN stroke thickness.
    paint->thickness(std::numeric_limits<float>::quiet_NaN());
    renderer.drawPath(path.get(), paint.get());

    // Check infinite stroke thickness.
    paint->thickness(std::numeric_limits<float>::infinity());
    renderer.drawPath(path.get(), paint.get());

    // Check -infinite stroke thickness.
    paint->thickness(-std::numeric_limits<float>::infinity());
    renderer.drawPath(path.get(), paint.get());

    auto renderTarget = renderContext->static_impl_cast<RenderContextNULL>()->makeRenderTarget(
        s_frameDescriptor.renderTargetWidth,
        s_frameDescriptor.renderTargetHeight);
    renderContext->flush({.renderTarget = renderTarget.get()});
}

// When AABB::height() evaluates inf - inf, the result is nan.
TEST_CASE("nan-testcase", "RiveRenderer")
{
    std::unique_ptr<RenderContext> nullContext = RenderContextNULL::MakeContext();
    nullContext->beginFrame({
        .renderTargetWidth = 2279,
        .renderTargetHeight = 710,
    });

    auto path = nullContext->makeEmptyRenderPath();
    path->moveTo(0, std::numeric_limits<float>::infinity());
    path->lineTo(1, std::numeric_limits<float>::infinity());

    auto paint = nullContext->makeRenderPaint();

    RiveRenderer renderer(nullContext.get());
    renderer.save();
    renderer.align(Fit::fill, Alignment::center, {144, 77, 885, 168}, {0, 0, 100, 100});
    renderer.drawPath(path.get(), paint.get());
    renderer.restore();

    auto renderTarget =
        nullContext->static_impl_cast<RenderContextNULL>()->makeRenderTarget(2279, 710);
    nullContext->flush({.renderTarget = renderTarget.get()});
}

// This path was found by the fuzzer and triggered an assertion.
TEST_CASE("nan-testcase2", "RiveRenderer")
{
    std::unique_ptr<RenderContext> nullContext = RenderContextNULL::MakeContext();
    nullContext->beginFrame({
        .renderTargetWidth = 2279,
        .renderTargetHeight = 710,
    });

    auto inf = std::numeric_limits<float>::infinity();
    auto nan = std::numeric_limits<float>::quiet_NaN();
    auto path = nullContext->makeEmptyRenderPath();
    path->lineTo(1, -inf);
    path->moveTo(nan, -inf);

    auto paint = nullContext->makeRenderPaint();

    RiveRenderer renderer(nullContext.get());
    renderer.save();
    renderer.align(Fit::fill,
                   {0.260523f, 0.803141f},
                   {144.649948f, 77.102341f, 885.225891f, 168.455017f},
                   {0, 0, 100, 100});
    renderer.drawPath(path.get(), paint.get());
    renderer.restore();

    auto renderTarget =
        nullContext->static_impl_cast<RenderContextNULL>()->makeRenderTarget(2279, 710);
    nullContext->flush({.renderTarget = renderTarget.get()});
}

// Attempting to create a gradient with invalid stops returns null.
TEST_CASE("invalid-gradient-stops", "RiveRenderer")
{
    std::vector<ColorInt> colors(100);
    std::unique_ptr<RenderContext> nullContext = RenderContextNULL::MakeContext();
    auto checkGradients = [&](std::vector<float> stops, bool shouldSucceed) {
        if (stops.size() > colors.size())
        {
            return false;
        }
        if (static_cast<bool>(
                nullContext
                    ->makeLinearGradient(0, 0, 0, 0, colors.data(), stops.data(), stops.size())) !=
            shouldSucceed)
        {
            return false;
        }
        if (static_cast<bool>(
                nullContext
                    ->makeRadialGradient(0, 0, 0, colors.data(), stops.data(), stops.size())) !=
            shouldSucceed)
        {
            return false;
        }
        return true;
    };
    CHECK(checkGradients({0, .5, 1}, true));

    auto inf = std::numeric_limits<float>::infinity();
    auto nan = std::numeric_limits<float>::quiet_NaN();

    // Empty gradients are invalid.
    CHECK(checkGradients({0}, true));
    CHECK(checkGradients({1}, true));
    CHECK(checkGradients({}, false));

    // Gradients outside 0..1 are invalid
    CHECK(checkGradients({0, .5f, 1.1f}, false));
    CHECK(checkGradients({0, .5f, inf}, false));
    CHECK(checkGradients({0, .5f, nan}, false));
    CHECK(checkGradients({-.1f, .5f, 1}, false));
    CHECK(checkGradients({-inf, .5f, 1}, false));
    CHECK(checkGradients({nan, .5f, 1}, false));

    // Unordered gradients are invalid.
    CHECK(checkGradients({0, .4F, .5F, 1}, true));
    CHECK(checkGradients({0, .5F, .4F, 1}, false));
    CHECK(checkGradients({.4f, 0, .5f, 1}, false));
    CHECK(checkGradients({0, .4f, 1, .5f}, false));
    CHECK(checkGradients({nan, .5f, 1}, false));
    CHECK(checkGradients({0, nan, 1}, false));
    CHECK(checkGradients({0, .5f, nan}, false));
}

// Drawing round caps with tiny, negative, and non-finite stroke thickness values should not crash.
TEST_CASE("round-cap-edge-values", "RiveRenderer")
{
    std::unique_ptr<RenderContext> renderContext = RenderContextNULL::MakeContext();

    renderContext->beginFrame(s_frameDescriptor);

    auto path = renderContext->makeEmptyRenderPath();
    path->moveTo(25, 25);
    path->lineTo(75, 75);

    auto emptyPath = renderContext->makeEmptyRenderPath();
    emptyPath->close();
    emptyPath->moveTo(50, 50);
    emptyPath->close();

    RiveRenderer renderer(renderContext.get());

    auto drawStrokes = [&](float thickness) {
        auto stroke = renderContext->makeRenderPaint();
        stroke->style(RenderPaintStyle::stroke);
        stroke->thickness(thickness);
        stroke->cap(StrokeCap::round);
        stroke->join(StrokeJoin::round);
        renderer.drawPath(path.get(), stroke.get());
        renderer.drawPath(emptyPath.get(), stroke.get());
    };

    for (float thickness = 1; thickness > 0; thickness /= 2)
    {
        drawStrokes(thickness);
        drawStrokes(-thickness);
    }
    drawStrokes(math::bit_cast<float>(1u));
    drawStrokes(std::numeric_limits<float>::infinity());
    drawStrokes(-std::numeric_limits<float>::infinity());
    drawStrokes(std::numeric_limits<float>::quiet_NaN());

    auto renderTarget = renderContext->static_impl_cast<RenderContextNULL>()->makeRenderTarget(
        s_frameDescriptor.renderTargetWidth,
        s_frameDescriptor.renderTargetHeight);
    renderContext->flush({.renderTarget = renderTarget.get()});
}

// Infinite draw bounds cast to {min32i, min32i, max32i, max32i}.
// Make sure these bounds don't overflow and cause assertions when working with the intersection
// board.
TEST_CASE("infinite-atomic-path", "RiveRenderer")
{
    std::unique_ptr<RenderContext> renderContext = RenderContextNULL::MakeContext();
    auto desc = s_frameDescriptor;
    desc.disableRasterOrdering = true;
    renderContext->beginFrame(desc);

    RiveRenderer renderer(renderContext.get());

    auto paint = renderContext->makeRenderPaint();
    auto path = renderContext->makeEmptyRenderPath();
    path->moveTo(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity());
    path->lineTo(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity());
    renderer.drawPath(path.get(), paint.get());

    auto renderTarget = renderContext->static_impl_cast<RenderContextNULL>()->makeRenderTarget(
        desc.renderTargetWidth,
        desc.renderTargetHeight);
    renderContext->flush({.renderTarget = renderTarget.get()});
}

} // namespace rive::gpu

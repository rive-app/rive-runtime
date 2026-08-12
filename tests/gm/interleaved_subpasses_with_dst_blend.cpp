/*
 * Copyright 2026 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"

#ifdef WITH_RIVE_TEXT

#include "assets/roboto_flex.ttf.hpp"
#include "common/testing_window.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text/raw_text.hpp"
#include "rive/renderer/render_context.hpp"

#ifndef RIVE_TOOLS_NO_GL
#include "rive/renderer/gl/render_context_gl_impl.hpp"
#endif

using namespace rive;
using namespace rivegm;

constexpr static uint32_t WINDOW_SIZE = 1600;

// We caught a bug where draws with multiple subpasses were being interleaved
// together like this:
//
//   - drawA, subpass0
//   - dstBlendBarrier (because drawB has a dstBlend)
//   - drawB, subpass0
//   - drawA, subpass1
//   - drawB, subpass1
//
// This was a problem because drawA was getting a dstBlend barrier between
// subpasses. When dstBlend is implemented as a texture copy, it interrupts the
// render pass and resolves MSAA, causing the MSAA data to be lost between
// supbasses of drawA.
//
// Since drawA and drawB don't overlap, the correct solution is to only apply
// barriers on the first batch of a drawGroup:
//
//   - dstBlendBarrier (because drawB has a dstBlend)
//   - drawA, subpass0
//   - drawB, subpass0
//   - drawA, subpass1
//   - drawB, subpass1
//
//
// (This also leads to fewer barriers overall.)
class InterleavedSubpassesGM : public rivegm::GM
{
public:
    InterleavedSubpassesGM() : GM(WINDOW_SIZE, WINDOW_SIZE) {}
    ColorInt clearColor() const override { return 0xff000000; }
    void updateFrameOptions(TestingWindow::FrameOptions* options) const override
    {
        // Both GMs here draw text paths complex enough to exceed the default
        // 256-verb triangulation guard. They exist to exercise interleaved
        // subpasses against interior triangulation, so lift the cap and keep
        // them on that path.
        options->triangulationThresholds.maxVerbs = SIZE_MAX;
    }
};

class interleaved_subpasses_with_dst_blend_GM : public InterleavedSubpassesGM
{
public:
    void onDraw(rive::Renderer*) override;
};
GMREGISTER(interleaved_subpasses_with_dst_blend,
           return new interleaved_subpasses_with_dst_blend_GM)
void interleaved_subpasses_with_dst_blend_GM::onDraw(rive::Renderer* renderer)
{
    gpu::RenderContext* renderContext = TestingWindow::Get()->renderContext();
    gpu::RenderContext::FrameDescriptor preserveRenderTargetFrameDesc;
    if (renderContext != nullptr)
    {
        preserveRenderTargetFrameDesc = renderContext->frameDescriptor();
        preserveRenderTargetFrameDesc.loadAction =
            gpu::LoadAction::preserveRenderTarget;
    }

    // Disable KHR_blend_equation_advanced, which is necessary for this to
    // repro.
#ifndef RIVE_TOOLS_NO_GL
    bool hadBlendAdvancedCoherentKHR = false;
    bool hadBlendAdvancedKHR = false;
    if (renderContext != nullptr)
    {
        if (gpu::RenderContextGLImpl* glImpl =
                TestingWindow::Get()->renderContextGLImpl())
        {
            TestingWindow::Get()->flushPLSContext();
            hadBlendAdvancedCoherentKHR =
                glImpl->testingOnly_setBlendAdvancedCoherentKHRSupported(false);
            hadBlendAdvancedKHR =
                glImpl->testingOnly_setBlendAdvancedKHRSupported(false);
            renderContext->beginFrame(preserveRenderTargetFrameDesc);
        }
    }
#endif

    {
        AutoRestore ar(renderer, true);

        auto roboto = HBFont::Decode(assets::roboto_flex_ttf());

        // Make sure the paint is slightly transparent so the path doesn't turn
        // into an opaque prepass. Its subpasses need to interleave with the
        // advanced blend draw.
        Paint white;
        white->color(0xeeffffff);

        RawText text(TestingWindow::Get()->factory());
        text.maxWidth(800);
        text.sizing(TextSizing::fixed);
        text.append("abcdefghijklmnopqrstuvwxyz@",
                    ref_rcp(white.get()),
                    roboto,
                    150);

        renderer->transform(
            Mat2D(1.539983f, 0, 0, 1.539983f, 292.155853f, 600));
        text.render(renderer);
    }

    {
        // Drawing a non-intersecting path with a blend mode is what triggers
        // the issue.
        auto renderPath =
            PathBuilder(FillRule::clockwise).addOval({0, 0, 1, 1}).detach();

        Paint renderPaint;
        renderPaint->color(0xfff3da93);
        renderPaint->blendMode(BlendMode::lighten);

        AutoRestore ar(renderer, true);
        renderer->transform(Mat2D(800, 0, 0, 300, 100, 100));
        renderer->drawPath(renderPath.get(), renderPaint.get());
    }

    // Restore KHR_blend_equation_advanced.
    if (renderContext != nullptr)
    {
#ifndef RIVE_TOOLS_NO_GL
        if (gpu::RenderContextGLImpl* glImpl =
                TestingWindow::Get()->renderContextGLImpl())
        {
            TestingWindow::Get()->flushPLSContext();
            glImpl->testingOnly_setBlendAdvancedCoherentKHRSupported(
                hadBlendAdvancedCoherentKHR);
            glImpl->testingOnly_setBlendAdvancedKHRSupported(
                hadBlendAdvancedKHR);
            renderContext->beginFrame(preserveRenderTargetFrameDesc);
        }
#endif
    }
}

class interleaved_subpasses_with_dst_blend2_GM : public InterleavedSubpassesGM
{
public:
    void onDraw(rive::Renderer*) override;
};
GMREGISTER(interleaved_subpasses_with_dst_blend2,
           return new interleaved_subpasses_with_dst_blend2_GM)
void interleaved_subpasses_with_dst_blend2_GM::onDraw(rive::Renderer* renderer)
{
    gpu::RenderContext* renderContext = TestingWindow::Get()->renderContext();
    gpu::RenderContext::FrameDescriptor preserveRenderTargetFrameDesc;
    if (renderContext != nullptr)
    {
        preserveRenderTargetFrameDesc = renderContext->frameDescriptor();
        preserveRenderTargetFrameDesc.loadAction =
            gpu::LoadAction::preserveRenderTarget;
    }

    // Disable KHR_blend_equation_advanced, which is necessary for this to
    // repro.
#ifndef RIVE_TOOLS_NO_GL
    bool hadBlendAdvancedCoherentKHR = false;
    bool hadBlendAdvancedKHR = false;
    if (renderContext != nullptr)
    {
        if (gpu::RenderContextGLImpl* glImpl =
                TestingWindow::Get()->renderContextGLImpl())
        {
            TestingWindow::Get()->flushPLSContext();
            hadBlendAdvancedCoherentKHR =
                glImpl->testingOnly_setBlendAdvancedCoherentKHRSupported(false);
            hadBlendAdvancedKHR =
                glImpl->testingOnly_setBlendAdvancedKHRSupported(false);
            renderContext->beginFrame(preserveRenderTargetFrameDesc);
        }
    }
#endif

    {
        AutoRestore ar(renderer, true);

        Paint paint;
        paint->color(0xe0da93f3);
        paint->feather(30.000000);
        paint->blendMode(BlendMode::srcOver);

        auto renderPath =
            PathBuilder(FillRule::clockwise)
                .addRect({0.099000f, -74.448006f, 656.115417f, 0.198000f})
                .detach();
        renderer->transform(Mat2D(1.506361f,
                                  0.000000f,
                                  0.000000f,
                                  1.506361f,
                                  303.258057f,
                                  1075.497070f));

        renderer->drawPath(renderPath.get(), paint.get());

        paint->color(0x93e0daf3);
        paint->feather(0);

        renderer->translate(-12, -12);
        renderer->drawPath(renderPath.get(), paint.get());
    }

    {
        AutoRestore ar(renderer, true);

        auto roboto = HBFont::Decode(assets::roboto_flex_ttf());

        // Make sure the paint is slightly transparent so the path doesn't turn
        // into an opaque prepass. Its subpasses need to interleave with the
        // advanced blend draw.
        Paint white;
        white->color(0xeeffffff);

        RawText text(TestingWindow::Get()->factory());
        text.maxWidth(800);
        text.sizing(TextSizing::fixed);
        text.append("abcdefghijklmnopqrstuvwxyz@",
                    ref_rcp(white.get()),
                    roboto,
                    75);

        renderer->transform(Mat2D(1.506361f,
                                  0.000000f,
                                  0.000000f,
                                  1.506361f,
                                  153.994843f,
                                  1209.160156f));
        text.render(renderer);
    }

    {
        AutoRestore ar(renderer, true);

        Paint paint;
        paint->color(0xfff3da93);
        paint->feather(30.000000);
        paint->blendMode(BlendMode::lighten);

        auto renderPath =
            PathBuilder(FillRule::clockwise)
                .addOval({-35.099998f, -48.600010f, 35.099998f, 48.600010f})
                .detach();

        renderer->transform(Mat2D(4.071247f,
                                  0.000000f,
                                  0.000000f,
                                  4.071247f,
                                  1159.308716f,
                                  537.785034f));

        renderer->drawPath(renderPath.get(), paint.get());
    }

    // Restore KHR_blend_equation_advanced.
    if (renderContext != nullptr)
    {
#ifndef RIVE_TOOLS_NO_GL
        if (gpu::RenderContextGLImpl* glImpl =
                TestingWindow::Get()->renderContextGLImpl())
        {
            TestingWindow::Get()->flushPLSContext();
            glImpl->testingOnly_setBlendAdvancedCoherentKHRSupported(
                hadBlendAdvancedCoherentKHR);
            glImpl->testingOnly_setBlendAdvancedKHRSupported(
                hadBlendAdvancedKHR);
            renderContext->beginFrame(preserveRenderTargetFrameDesc);
        }
#endif
    }
}

#endif // WITH_RIVE_TEXT

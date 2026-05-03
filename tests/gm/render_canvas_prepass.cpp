/*
 * Copyright 2025 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "common/testing_window.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/render_context_impl.hpp"
#include "rive/renderer/rive_renderer.hpp"

using namespace rivegm;
using namespace rive;
using namespace rive::gpu;

// Verifies the canvas pre-pass pattern used by ScriptedCanvas: the canvas
// renders into its own beginFrame/flush cycle (with its own command buffer
// created via makeCommandBuffer/commitCommandBuffer) BEFORE the main frame
// opens. The main frame then composites the canvas image.
//
// This is the pattern that all platform runtimes use to avoid the
// !m_didBeginFrame assertion when scripted canvas draws happen before the
// host render loop's beginFrame().
class RenderCanvasPrepass : public GM
{
public:
    RenderCanvasPrepass() : GM(256, 256) {}

    ColorInt clearColor() const override { return 0xffff0000; } // red

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext)
            return;

        auto canvas = renderContext->makeRenderCanvas(256, 256);
        if (!canvas)
            return;

        // --- Canvas pre-pass (before the main frame) ---
        // Flush the current main frame so the context is idle.
        auto originalFrameDescriptor = renderContext->frameDescriptor();
        TestingWindow::Get()->flushPLSContext();

        // Create a command buffer via the impl, just like ScriptedCanvas does.
        void* commandBuffer = renderContext->impl()->makeCommandBuffer();

        // Open a canvas-only frame.
        auto canvasFD = originalFrameDescriptor;
        canvasFD.renderTargetWidth = canvas->width();
        canvasFD.renderTargetHeight = canvas->height();
        canvasFD.clearColor = 0xff0000ff; // blue background
        canvasFD.loadAction = gpu::LoadAction::clear;
        renderContext->beginFrame(std::move(canvasFD));

        // Draw a green circle into the canvas.
        RiveRenderer canvasRenderer(renderContext);
        Paint green(0xff00ff00);
        PathBuilder builder;
        int nsegs = 40;
        float r = 80;
        float cx = 128, cy = 128;
        for (int i = 0; i <= nsegs; ++i)
        {
            float theta = 2 * math::PI * i / nsegs;
            float x = cx + r * cosf(theta);
            float y = cy + r * sinf(theta);
            if (i == 0)
                builder.moveTo(x, y);
            else
                builder.lineTo(x, y);
        }
        canvasRenderer.drawPath(builder.detach(), green);

        // Flush to the canvas render target with the command buffer.
        RenderContext::FlushResources canvasFlush{};
        canvasFlush.renderTarget = canvas->renderTarget();
        canvasFlush.externalCommandBuffer = commandBuffer;
        renderContext->flush(canvasFlush);

        // Commit the command buffer (submits GPU work).
        renderContext->impl()->commitCommandBuffer(commandBuffer);

        // --- Main frame resumes ---
        auto mainFD = originalFrameDescriptor;
        mainFD.loadAction = gpu::LoadAction::preserveRenderTarget;
        renderContext->beginFrame(std::move(mainFD));

        // Composite the canvas into the main framebuffer.
        RiveRenderer mainRenderer(renderContext);
        mainRenderer.save();
        if (renderContext->platformFeatures().framebufferBottomUp)
        {
            mainRenderer.translate(0, 256);
            mainRenderer.scale(1, -1);
        }
        mainRenderer.drawImage(canvas->renderImage(),
                               {.filter = ImageFilter::nearest},
                               BlendMode::srcOver,
                               1);
        mainRenderer.restore();
    }
};
GMREGISTER(render_canvas_prepass, return new RenderCanvasPrepass())

// Verifies that multiple canvases can render sequentially using the pre-pass
// pattern. Each canvas gets its own beginFrame/flush cycle with its own
// command buffer, then all are composited in the main frame.
class RenderCanvasPrepassMulti : public GM
{
public:
    RenderCanvasPrepassMulti() : GM(256, 256) {}

    ColorInt clearColor() const override { return 0xff000000; } // black

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext)
            return;

        auto canvasA = renderContext->makeRenderCanvas(128, 128);
        auto canvasB = renderContext->makeRenderCanvas(128, 128);
        if (!canvasA || !canvasB)
            return;

        auto originalFrameDescriptor = renderContext->frameDescriptor();
        TestingWindow::Get()->flushPLSContext();

        // --- Canvas A: red circle on blue ---
        {
            void* cmdBuf = renderContext->impl()->makeCommandBuffer();
            auto fd = originalFrameDescriptor;
            fd.renderTargetWidth = canvasA->width();
            fd.renderTargetHeight = canvasA->height();
            fd.clearColor = 0xff0000ff; // blue
            fd.loadAction = gpu::LoadAction::clear;
            renderContext->beginFrame(std::move(fd));

            RiveRenderer renderer(renderContext);
            Paint red(0xffff0000);
            draw_oval(&renderer, AABB{8, 8, 120, 120}, red);

            RenderContext::FlushResources flush{};
            flush.renderTarget = canvasA->renderTarget();
            flush.externalCommandBuffer = cmdBuf;
            renderContext->flush(flush);
            renderContext->impl()->commitCommandBuffer(cmdBuf);
        }

        // --- Canvas B: green rect on yellow ---
        {
            void* cmdBuf = renderContext->impl()->makeCommandBuffer();
            auto fd = originalFrameDescriptor;
            fd.renderTargetWidth = canvasB->width();
            fd.renderTargetHeight = canvasB->height();
            fd.clearColor = 0xffffff00; // yellow
            fd.loadAction = gpu::LoadAction::clear;
            renderContext->beginFrame(std::move(fd));

            RiveRenderer renderer(renderContext);
            Paint green(0xff00ff00);
            draw_rect(&renderer, AABB{16, 16, 112, 112}, green);

            RenderContext::FlushResources flush{};
            flush.renderTarget = canvasB->renderTarget();
            flush.externalCommandBuffer = cmdBuf;
            renderContext->flush(flush);
            renderContext->impl()->commitCommandBuffer(cmdBuf);
        }

        // --- Main frame: composite both canvases ---
        auto mainFD = originalFrameDescriptor;
        mainFD.loadAction = gpu::LoadAction::preserveRenderTarget;
        renderContext->beginFrame(std::move(mainFD));

        RiveRenderer mainRenderer(renderContext);
        mainRenderer.save();
        bool bottomUp = renderContext->platformFeatures().framebufferBottomUp;

        // Canvas A in top-left quadrant
        mainRenderer.save();
        if (bottomUp)
        {
            mainRenderer.translate(0, 256);
            mainRenderer.scale(1, -1);
        }
        mainRenderer.drawImage(canvasA->renderImage(),
                               {.filter = ImageFilter::nearest},
                               BlendMode::srcOver,
                               1);
        mainRenderer.restore();

        // Canvas B in bottom-right quadrant
        mainRenderer.save();
        mainRenderer.translate(128, 128);
        if (bottomUp)
        {
            mainRenderer.scale(1, -1);
        }
        mainRenderer.drawImage(canvasB->renderImage(),
                               {.filter = ImageFilter::nearest},
                               BlendMode::srcOver,
                               1);
        mainRenderer.restore();

        mainRenderer.restore();
    }
};
GMREGISTER(render_canvas_prepass_multi, return new RenderCanvasPrepassMulti())

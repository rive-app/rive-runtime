/*
 * Copyright 2025 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "common/testing_window.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/rive_renderer.hpp"

using namespace rivegm;
using namespace rive;
using namespace rive::gpu;

// Renders Rive 2D content into a RenderCanvas and composites the result into
// the main framebuffer. Verifies the basic render-to-texture lifecycle:
// makeRenderCanvas() -> renderTarget() -> beginFrame/flush -> renderImage() ->
// drawImage.
class RenderCanvasBasic : public GM
{
public:
    RenderCanvasBasic() : GM(256, 256) {}

    ColorInt clearColor() const override { return 0xffff0000; } // red

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext)
            return;

        auto canvas = renderContext->makeRenderCanvas(256, 256);
        if (!canvas)
            return;

        // Intercept the current frame and flush.
        auto originalFrameDescriptor = renderContext->frameDescriptor();
        TestingWindow::Get()->flushPLSContext();

        // Begin a new frame targeting the canvas. Declare the frame
        // dimensions explicitly so Rive sizes its PLS buffers for the
        // canvas render target we are about to flush into. Without this,
        // backends whose main frame descriptor is larger than the canvas
        // (e.g. wagyu where the surface exceeds the GM size) trip the
        // flushResources.renderTarget->width() ==
        // m_frameDescriptor.renderTargetWidth assertion in
        // RenderContext::flush.
        auto canvasFrameDescriptor = originalFrameDescriptor;
        canvasFrameDescriptor.renderTargetWidth = canvas->width();
        canvasFrameDescriptor.renderTargetHeight = canvas->height();
        canvasFrameDescriptor.clearColor = 0xff0000ff; // blue
        renderContext->beginFrame(std::move(canvasFrameDescriptor));

        // Draw a green circle into the canvas.
        RiveRenderer renderer(renderContext);
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
        renderer.drawPath(builder.detach(), green);

        // Flush to the canvas's render target.
        TestingWindow::Get()->flushPLSContext(canvas->renderTarget());

        // Resume the main frame and composite the canvas.
        auto mainFrameDescriptor = originalFrameDescriptor;
        mainFrameDescriptor.loadAction = gpu::LoadAction::preserveRenderTarget;
        renderContext->beginFrame(std::move(mainFrameDescriptor));
        renderer.save();
        if (renderContext->platformFeatures().framebufferBottomUp)
        {
            renderer.translate(0, 256);
            renderer.scale(1, -1);
        }
        renderer.drawImage(canvas->renderImage(),
                           {.filter = ImageFilter::nearest},
                           BlendMode::srcOver,
                           1);
        renderer.restore();
    }
};
GMREGISTER(render_canvas_basic, return new RenderCanvasBasic())

// Verifies that RenderCanvas content persists across frames. Renders a green
// circle into the canvas on the first frame, then on the second frame only
// composites the canvas (without re-rendering into it).
class RenderCanvasPersistence : public GM
{
public:
    RenderCanvasPersistence() : GM(256, 256) {}

    ColorInt clearColor() const override { return 0xffff0000; } // red

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext)
            return;

        if (!m_canvas)
        {
            m_canvas = renderContext->makeRenderCanvas(256, 256);
            if (!m_canvas)
                return;

            // Render green circle into the canvas on first call.
            auto originalFrameDescriptor = renderContext->frameDescriptor();
            TestingWindow::Get()->flushPLSContext();

            auto canvasFD = originalFrameDescriptor;
            canvasFD.renderTargetWidth = m_canvas->width();
            canvasFD.renderTargetHeight = m_canvas->height();
            canvasFD.clearColor = 0xff0000ff; // blue
            renderContext->beginFrame(std::move(canvasFD));
            RiveRenderer renderer(renderContext);
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
            renderer.drawPath(builder.detach(), green);
            TestingWindow::Get()->flushPLSContext(m_canvas->renderTarget());

            // Resume main frame.
            auto mainFD = originalFrameDescriptor;
            mainFD.loadAction = gpu::LoadAction::preserveRenderTarget;
            renderContext->beginFrame(std::move(mainFD));
        }

        // Composite the canvas (rendered on first frame, persisted on later
        // frames).
        RiveRenderer renderer(renderContext);
        renderer.save();
        if (renderContext->platformFeatures().framebufferBottomUp)
        {
            renderer.translate(0, 256);
            renderer.scale(1, -1);
        }
        renderer.drawImage(m_canvas->renderImage(),
                           {.filter = ImageFilter::nearest},
                           BlendMode::srcOver,
                           1);
        renderer.restore();
    }

private:
    rcp<RenderCanvas> m_canvas;
};
GMREGISTER(render_canvas_persistence, return new RenderCanvasPersistence())

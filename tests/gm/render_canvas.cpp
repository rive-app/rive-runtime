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
        renderer.drawImage(canvas->renderImage(),
                           {.filter = ImageFilter::nearest},
                           BlendMode::srcOver,
                           1);
    }
};
GMREGISTER(render_canvas_basic, return new RenderCanvasBasic())

// Composites a RenderCanvas through drawImageMesh, whose UVs come straight
// from the mesh buffers instead of an image matrix. The circle sits high so a
// vertically mirrored composite is visible.
class RenderCanvasMesh : public GM
{
public:
    RenderCanvasMesh() : GM(256, 256) {}

    ColorInt clearColor() const override { return 0xffff0000; } // red

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext)
            return;

        auto canvas = renderContext->makeRenderCanvas(256, 256);
        if (!canvas)
            return;

        auto originalFrameDescriptor = renderContext->frameDescriptor();
        TestingWindow::Get()->flushPLSContext();

        auto canvasFrameDescriptor = originalFrameDescriptor;
        canvasFrameDescriptor.renderTargetWidth = canvas->width();
        canvasFrameDescriptor.renderTargetHeight = canvas->height();
        canvasFrameDescriptor.clearColor = 0xff0000ff; // blue
        renderContext->beginFrame(std::move(canvasFrameDescriptor));

        RiveRenderer renderer(renderContext);
        Paint green(0xff00ff00);
        PathBuilder builder;
        int nsegs = 40;
        float r = 60;
        float cx = 128, cy = 80;
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
        TestingWindow::Get()->flushPLSContext(canvas->renderTarget());

        auto mainFrameDescriptor = originalFrameDescriptor;
        mainFrameDescriptor.loadAction = gpu::LoadAction::preserveRenderTarget;
        renderContext->beginFrame(std::move(mainFrameDescriptor));

        Factory* factory = TestingWindow::Get()->factory();
        auto pts = factory->makeRenderBuffer(
            RenderBufferType::vertex,
            RenderBufferFlags::mappedOnceAtInitialization,
            4 * sizeof(Vec2D));
        memcpy(pts->map(),
               std::array<Vec2D, 4>{Vec2D{0, 0},
                                    Vec2D{256, 0},
                                    Vec2D{0, 256},
                                    Vec2D{256, 256}}
                   .data(),
               pts->sizeInBytes());
        pts->unmap();
        auto uvs = factory->makeRenderBuffer(
            RenderBufferType::vertex,
            RenderBufferFlags::mappedOnceAtInitialization,
            4 * sizeof(Vec2D));
        memcpy(uvs->map(),
               std::array<Vec2D, 4>{Vec2D{0, 0},
                                    Vec2D{1, 0},
                                    Vec2D{0, 1},
                                    Vec2D{1, 1}}
                   .data(),
               uvs->sizeInBytes());
        uvs->unmap();
        auto indices = factory->makeRenderBuffer(
            RenderBufferType::index,
            RenderBufferFlags::mappedOnceAtInitialization,
            6 * sizeof(uint16_t));
        memcpy(indices->map(),
               std::array<uint16_t, 6>{0, 1, 2, 1, 2, 3}.data(),
               indices->sizeInBytes());
        indices->unmap();
        renderer.drawImageMesh(canvas->renderImage(),
                               {.filter = ImageFilter::nearest},
                               pts,
                               uvs,
                               indices,
                               4,
                               6,
                               BlendMode::srcOver,
                               1);
    }
};
GMREGISTER(render_canvas_mesh, return new RenderCanvasMesh())

// Winding sensitive content inside a canvas: a clockwise fill rule keeps the
// clockwise square and drops the counter clockwise one, under a circular clip.
// A target whose rows run the other way from the window inverts winding, so
// the squares swap if the front face is not set per target.
class RenderCanvasWinding : public GM
{
public:
    RenderCanvasWinding() : GM(256, 256) {}

    ColorInt clearColor() const override { return 0xffff0000; } // red

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext)
            return;

        auto canvas = renderContext->makeRenderCanvas(256, 256);
        if (!canvas)
            return;

        auto originalFrameDescriptor = renderContext->frameDescriptor();
        TestingWindow::Get()->flushPLSContext();

        auto canvasFrameDescriptor = originalFrameDescriptor;
        canvasFrameDescriptor.renderTargetWidth = canvas->width();
        canvasFrameDescriptor.renderTargetHeight = canvas->height();
        canvasFrameDescriptor.clearColor = 0xff0000ff; // blue
        renderContext->beginFrame(std::move(canvasFrameDescriptor));

        RiveRenderer renderer(renderContext);
        // The clip stack outlives the flush, so keep it out of the composite.
        renderer.save();
        Path clip = PathBuilder::Circle(128, 128, 110);
        renderer.clipPath(clip.get());
        Paint green(0xff00ff00);
        renderer.drawPath(PathBuilder(FillRule::clockwise)
                              .moveTo(24, 24)
                              .lineTo(120, 24)
                              .lineTo(120, 120)
                              .lineTo(24, 120)
                              .close()
                              .detach(),
                          green);
        Paint yellow(0xffffff00);
        renderer.drawPath(PathBuilder(FillRule::clockwise)
                              .moveTo(136, 136)
                              .lineTo(136, 232)
                              .lineTo(232, 232)
                              .lineTo(232, 136)
                              .close()
                              .detach(),
                          yellow);
        renderer.restore();
        TestingWindow::Get()->flushPLSContext(canvas->renderTarget());

        auto mainFrameDescriptor = originalFrameDescriptor;
        mainFrameDescriptor.loadAction = gpu::LoadAction::preserveRenderTarget;
        renderContext->beginFrame(std::move(mainFrameDescriptor));
        renderer.drawImage(canvas->renderImage(),
                           {.filter = ImageFilter::nearest},
                           BlendMode::srcOver,
                           1);
    }
};
GMREGISTER(render_canvas_winding, return new RenderCanvasWinding())

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
        renderer.drawImage(m_canvas->renderImage(),
                           {.filter = ImageFilter::nearest},
                           BlendMode::srcOver,
                           1);
    }

private:
    rcp<RenderCanvas> m_canvas;
};
GMREGISTER(render_canvas_persistence, return new RenderCanvasPersistence())

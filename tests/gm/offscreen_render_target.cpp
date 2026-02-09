/*
 * Copyright 2024 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "common/testing_window.hpp"
#include "common/offscreen_render_target.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/rive_renderer.hpp"

#ifdef RIVE_TOOLS_NO_GL
using GLuint = uint32_t;
#else
#include "rive/renderer/gl/render_context_gl_impl.hpp"
#include "rive/renderer/gl/render_target_gl.hpp"
#endif

#include "assets/nomoon.png.hpp"

using namespace rivegm;
using namespace rive;
using namespace rive::gpu;

// Most gms render directly to the framebuffer. This GM checks that texture
// targets work in GL.
class OffscreenRenderTarget : public GM
{
public:
    OffscreenRenderTarget(BlendMode blendMode,
                          bool riveRenderable,
                          ColorInt clearColor = 0xffff00ff) :
        GM(256, 256),
        m_blendMode(blendMode),
        m_riveRenderable(riveRenderable),
        m_clearColor(clearColor)
    {}

    ColorInt clearColor() const override { return 0xffff0000; }

    void onDraw(rive::Renderer* originalRenderer) override
    {
        if (rcp<rive_tests::OffscreenRenderTarget> renderImageTarget =
                TestingWindow::Get()
                    ->makeOffscreenRenderTarget(256, 256, m_riveRenderable))
        {
            // Intercept the current frameDescriptor and end the PLS frame.
            auto renderContext = TestingWindow::Get()->renderContext();
            auto originalFrameDescriptor = renderContext->frameDescriptor();
            TestingWindow::Get()->flushPLSContext();

            // Draw to the offscreen texture.
            auto textureFrameDescriptor = originalFrameDescriptor;
            textureFrameDescriptor.clearColor = m_clearColor;
            renderContext->beginFrame(std::move(textureFrameDescriptor));
            RiveRenderer renderer(renderContext);
            drawInternal(&renderer, renderImageTarget->asRenderTarget());
            TestingWindow::Get()->flushPLSContext(
                renderImageTarget->asRenderTarget());

            // Copy the offscreen texture back to the destination framebuffer.
            RenderImage* renderImage = renderImageTarget->asRenderImage();
            auto copyFrameDescriptor = originalFrameDescriptor;
            copyFrameDescriptor.loadAction =
                gpu::LoadAction::preserveRenderTarget;
            copyFrameDescriptor.clearColor = 0xffff0000;
            renderContext->beginFrame(std::move(copyFrameDescriptor));
            renderer.save();
            if (renderContext->platformFeatures().framebufferBottomUp)
            {
                renderer.translate(0, 256);
                renderer.scale(1, -1);
            }
            renderer.drawImage(renderImage,
                               {.filter = ImageFilter::nearest},
                               BlendMode::srcOver,
                               1);
            renderer.restore();
        }
        else
        {
            drawInternal(originalRenderer, nullptr);
        }
    }

    virtual void drawInternal(Renderer* renderer, RenderTarget*)
    {
        Paint paint5(0x8000ffff);
        paint5->blendMode(m_blendMode);
        drawStar5(renderer, paint5);

        Paint paint13(0x80ffff00);
        paint13->blendMode(m_blendMode);
        drawStar13(renderer, paint13);
    }

    void drawStar5(Renderer* renderer,
                   RenderPaint* paint,
                   FillRule fillRule = FillRule::nonZero)
    {
        PathBuilder builder;
        float theta = -math::PI / 7;
        builder.moveTo(cosf(theta), sinf(theta));
        for (int i = 0; i <= 7; ++i)
        {
            theta += 2 * math::PI * 2 / 7;
            builder.lineTo(cosf(theta), sinf(theta));
        }
        builder.fillRule(fillRule);
        renderer->save();
        renderer->translate(100, 100);
        renderer->scale(80, 80);
        renderer->drawPath(builder.detach(), paint);
        renderer->restore();
    }

    void drawStar13(Renderer* renderer,
                    RenderPaint* paint,
                    FillRule fillRule = FillRule::evenOdd)
    {
        PathBuilder builder;
        float theta = 0;
        for (int i = 0; i <= 13; ++i)
        {
            theta += 2 * math::PI * 3 / 13;
            builder.lineTo(cosf(theta), sinf(theta));
        }
        builder.fillRule(fillRule);
        renderer->save();
        renderer->translate(256 - 100, 256 - 100);
        renderer->scale(80, 80);
        renderer->drawPath(builder.detach(), paint);
        renderer->restore();
    }

protected:
    const BlendMode m_blendMode;
    const bool m_riveRenderable;
    const ColorInt m_clearColor;
};

GMREGISTER(offscreen_render_target,
           return new OffscreenRenderTarget(BlendMode::srcOver, true))
GMREGISTER(offscreen_render_target_nonrenderable,
           return new OffscreenRenderTarget(BlendMode::srcOver, false))
GMREGISTER(offscreen_render_target_lum,
           return new OffscreenRenderTarget(BlendMode::luminosity, true))
GMREGISTER(offscreen_render_target_lum_nonrenderable,
           return new OffscreenRenderTarget(BlendMode::luminosity, false))

GMREGISTER(offscreen_render_target_transparent_clear,
           return new OffscreenRenderTarget(BlendMode::srcOver,
                                            true,
                                            0x7fff00ff))
GMREGISTER(offscreen_render_target_nonrenderable_transparent_clear,
           return new OffscreenRenderTarget(BlendMode::srcOver,
                                            false,
                                            0x7fff00ff))
GMREGISTER(offscreen_render_target_lum_transparent_clear,
           return new OffscreenRenderTarget(BlendMode::luminosity,
                                            true,
                                            0x7fff00ff))
GMREGISTER(offscreen_render_target_lum_nonrenderable_transparent_clear,
           return new OffscreenRenderTarget(BlendMode::luminosity,
                                            false,
                                            0x7fff00ff))

// This GM checks that texture targets (including MSAA targets) work with
// LoadAction::preserveRenderTarget.
class OffscreenRenderTargetPreserve : public OffscreenRenderTarget
{
public:
    OffscreenRenderTargetPreserve(BlendMode blendMode, bool riveRenderable) :
        OffscreenRenderTarget(blendMode, riveRenderable)
    {}
    OffscreenRenderTargetPreserve(bool riveRenderable) :
        OffscreenRenderTargetPreserve(BlendMode::srcOver, riveRenderable)
    {}

    virtual void drawInternal(Renderer* renderer, RenderTarget* renderTarget)
    {
        ColorInt colors[2];
        float stops[2];
        colors[0] = 0xff000000;
        stops[0] = 0;
        colors[1] = 0xffff00ff;
        stops[1] = 1;
        Paint paint;
        paint->shader(TestingWindow::Get()
                          ->factory()
                          ->makeLinearGradient(0, 0, 250, 0, colors, stops, 2));
        renderer->drawPath(PathBuilder::Rect({0, 0, 256, 256}), paint);

        colors[0] = 0x80000000;
        stops[0] = 0;
        colors[1] = 0x8000ffff;
        stops[1] = 1;
        Paint paint2;
        paint2->shader(
            TestingWindow::Get()
                ->factory()
                ->makeLinearGradient(0, 0, 0, 250, colors, stops, 2));
        renderer->drawPath(PathBuilder::Rect({0, 0, 256, 256}), paint2);

        if (auto renderContext = TestingWindow::Get()->renderContext())
        {
            auto frameDescriptor = renderContext->frameDescriptor();
#ifndef RIVE_TOOLS_NO_GL
            if (auto plsImplGL = TestingWindow::Get()->renderContextGLImpl())
            {
                assert(renderTarget);
                TestingWindow::Get()->flushPLSContext(renderTarget);
                if (int sampleCount = frameDescriptor.msaaSampleCount)
                {
                    // If the MSAA framebuffer target is not the target texture,
                    // wipe it to red behind the scenes in order to make sure
                    // our preservation codepath works. (It shouldn't appear red
                    // in the end -- this should get preserved instead.)
                    if (!plsImplGL->capabilities()
                             .EXT_multisampled_render_to_texture)
                    {
                        auto renderTextureTargetGL =
                            static_cast<RenderTargetGL*>(renderTarget);
                        renderTextureTargetGL->bindMSAAFramebuffer(plsImplGL,
                                                                   sampleCount,
                                                                   nullptr,
                                                                   nullptr);
                        glClearColor(1, 0, 0, 1);
                        glClear(GL_COLOR_BUFFER_BIT);
                    }
                }
            }
            else
#endif
            {
                TestingWindow::Get()->flushPLSContext(renderTarget);
            }
            frameDescriptor.loadAction = gpu::LoadAction::preserveRenderTarget;
            renderContext->beginFrame(std::move(frameDescriptor));
        }
        Paint paint5(0x8000ffff);
        paint5->blendMode(m_blendMode);
        drawStar5(renderer, paint5);

        if (auto renderContext = TestingWindow::Get()->renderContext())
        {
            auto frameDescriptor = renderContext->frameDescriptor();
#ifndef RIVE_TOOLS_NO_GL
            if (auto plsImplGL = TestingWindow::Get()->renderContextGLImpl())
            {
                assert(renderTarget);
                TestingWindow::Get()->flushPLSContext(renderTarget);
                if (int sampleCount = frameDescriptor.msaaSampleCount)
                {
                    // If the MSAA framebuffer target is not the target texture,
                    // wipe it to red behind the scenes in order to make sure
                    // our preservation codepath works. (It shouldn't appear red
                    // in the end -- this should get preserved instead.)
                    if (!plsImplGL->capabilities()
                             .EXT_multisampled_render_to_texture)
                    {
                        auto renderTextureTargetGL =
                            static_cast<RenderTargetGL*>(renderTarget);
                        renderTextureTargetGL->bindMSAAFramebuffer(plsImplGL,
                                                                   sampleCount,
                                                                   nullptr,
                                                                   nullptr);
                        glClearColor(1, 0, 0, 1);
                        glClear(GL_COLOR_BUFFER_BIT);
                    }
                }
            }
            else
#endif
            {
                TestingWindow::Get()->flushPLSContext(renderTarget);
            }
            frameDescriptor.loadAction = gpu::LoadAction::preserveRenderTarget;
            renderContext->beginFrame(std::move(frameDescriptor));
        }
        Paint paint13(0x80ffff00);
        paint13->blendMode(m_blendMode);
        drawStar13(renderer, paint13);
    }
};
GMREGISTER(offscreen_render_target_preserve,
           return new OffscreenRenderTargetPreserve(true))
GMREGISTER(offscreen_render_target_preserve_nonrenderable,
           return new OffscreenRenderTargetPreserve(false))

// ...And verify that blend modes work on a texture target.
class OffscreenRenderTargetPreserveLum : public OffscreenRenderTargetPreserve
{
public:
    OffscreenRenderTargetPreserveLum(bool riveRenderable) :
        OffscreenRenderTargetPreserve(BlendMode::luminosity, riveRenderable)
    {}
};
GMREGISTER(offscreen_render_target_preserve_lum,
           return new OffscreenRenderTargetPreserveLum(true))
GMREGISTER(offscreen_render_target_preserve_lum_nonrenderable,
           return new OffscreenRenderTargetPreserveLum(false))

// This GM checks that virtual tiles work with various types of render targets.
class OffscreenVirtualTiles : public OffscreenRenderTarget
{
public:
    OffscreenVirtualTiles(gpu::LoadAction loadAction, BlendMode blendMode) :
        OffscreenRenderTarget(blendMode, /*riveRenderable=*/false),
        m_loadAction(loadAction)
    {}

    virtual void drawInternal(Renderer* renderer, RenderTarget* renderTarget)
    {
        ColorInt colors[2];
        float stops[2];
        colors[0] = 0xff000000;
        stops[0] = 0;
        colors[1] = 0xffff00ff;
        stops[1] = 1;
        Paint paint;
        paint->shader(TestingWindow::Get()
                          ->factory()
                          ->makeLinearGradient(0, 0, 250, 0, colors, stops, 2));
        renderer->drawPath(PathBuilder::Rect({0, 0, 256, 256}), paint);

        colors[0] = 0x80000000;
        stops[0] = 0;
        colors[1] = 0x8000ffff;
        stops[1] = 1;
        Paint paint2;
        paint2->shader(
            TestingWindow::Get()
                ->factory()
                ->makeLinearGradient(0, 0, 0, 250, colors, stops, 2));
        renderer->drawPath(PathBuilder::Rect({0, 0, 256, 256}), paint2);

        if (auto renderContext = TestingWindow::Get()->renderContext())
        {
            auto frameDescriptor = renderContext->frameDescriptor();
            TestingWindow::Get()->flushPLSContext(renderTarget);
            frameDescriptor.loadAction = m_loadAction;
            frameDescriptor.virtualTileWidth = 37;
            frameDescriptor.virtualTileHeight = 23;
            renderContext->beginFrame(std::move(frameDescriptor));
        }

        Path largePathThatUsesInteriorTriangles;
        Paint transWhite;
        transWhite->color(0x80ffffff);
        transWhite->blendMode(m_blendMode);
        largePathThatUsesInteriorTriangles->addRect(0, 0, 1000, 1000);
        renderer->drawPath(largePathThatUsesInteriorTriangles.get(),
                           transWhite.get());

        Paint paint5(0x8000ffff);
        paint5->blendMode(m_blendMode);
        drawStar5(renderer, paint5, FillRule::clockwise);

        auto img = LoadImage(assets::nomoon_png());

        {
            AutoRestore ar(renderer, true);
            renderer->translate(100, 30);
            renderer->scale(.4f, .4f);
            renderer->drawImage(img.get(),
                                ImageSampler::LinearClamp(),
                                m_blendMode,
                                .5f);
        }

        {
            Factory* factory = TestingWindow::Get()->factory();
            auto pts = factory->makeRenderBuffer(
                RenderBufferType::vertex,
                RenderBufferFlags::mappedOnceAtInitialization,
                4 * sizeof(Vec2D));
            memcpy(pts->map(),
                   std::array<Vec2D, 4>{Vec2D{110, 110},
                                        Vec2D{200, 110},
                                        Vec2D{10, 200},
                                        Vec2D{270, 270}}
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
            renderer->drawImageMesh(img.get(),
                                    ImageSampler::LinearClamp(),
                                    pts,
                                    uvs,
                                    indices,
                                    4,
                                    6,
                                    m_blendMode,
                                    .7f);
        }

        Paint paint13(0x80ffff00);
        paint13->blendMode(m_blendMode);
        paint13->feather(1);
        drawStar13(renderer, paint13, FillRule::clockwise);

        Paint white(0xffffffff);
        paint13->blendMode(m_blendMode);
        uint64_t lcg = 12789;
        for (size_t i = 0; i < 2000; ++i)
        {
            AutoRestore ar(renderer, true);
            renderer->translate(lcg & 0xff, (lcg >> 8) & 0xff);
            renderer->scale(.01f, .01f);
            drawStar5(renderer, white, FillRule::clockwise);
            lcg = lcg * 6364136223846793005ULL + 1;
        }
    }

private:
    const gpu::LoadAction m_loadAction;
};
GMREGISTER(offscreen_virtual_tiles_nonrenderable,
           return new OffscreenVirtualTiles(gpu::LoadAction::clear,
                                            BlendMode::srcOver))
GMREGISTER(
    offscreen_virtual_tiles_preserve_nonrenderable,
    return new OffscreenVirtualTiles(gpu::LoadAction::preserveRenderTarget,
                                     BlendMode::srcOver))
GMREGISTER(offscreen_virtual_tiles_lum_nonrenderable,
           return new OffscreenVirtualTiles(gpu::LoadAction::clear,
                                            BlendMode::luminosity))
GMREGISTER(
    offscreen_virtual_tiles_preserve_lum_nonrenderable,
    return new OffscreenVirtualTiles(gpu::LoadAction::preserveRenderTarget,
                                     BlendMode::luminosity))

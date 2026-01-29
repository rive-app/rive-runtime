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

    void drawStar5(Renderer* renderer, RenderPaint* paint)
    {
        PathBuilder builder;
        float theta = -math::PI / 7;
        builder.moveTo(cosf(theta), sinf(theta));
        for (int i = 0; i <= 7; ++i)
        {
            theta += 2 * math::PI * 2 / 7;
            builder.lineTo(cosf(theta), sinf(theta));
        }
        renderer->save();
        renderer->translate(100, 100);
        renderer->scale(80, 80);
        renderer->drawPath(builder.detach(), paint);
        renderer->restore();
    }

    void drawStar13(Renderer* renderer, RenderPaint* paint)
    {
        PathBuilder builder;
        float theta = 0;
        for (int i = 0; i <= 13; ++i)
        {
            theta += 2 * math::PI * 3 / 13;
            builder.lineTo(cosf(theta), sinf(theta));
        }
        builder.fillRule(FillRule::evenOdd);
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

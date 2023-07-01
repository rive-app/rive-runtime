/*
 * Copyright 2023 Rive
 */

#include "rive/pls/gl/pls_render_context_gl.hpp"

#include "rive/pls/gl/pls_render_target_gl.hpp"

#include "../out/obj/generated/glsl.exports.h"

namespace rive::pls
{
constexpr static GLenum kPLSDrawBuffers[4] = {GL_COLOR_ATTACHMENT0,
                                              GL_COLOR_ATTACHMENT1,
                                              GL_COLOR_ATTACHMENT2,
                                              GL_COLOR_ATTACHMENT3};

class PLSRenderContextGL::PLSImplFramebufferFetch : public PLSRenderContextGL::PLSImpl
{
public:
    PLSImplFramebufferFetch(GLExtensions extensions) : m_extensions(extensions) {}

    rcp<PLSRenderTargetGL> wrapGLRenderTarget(GLuint framebufferID,
                                              size_t width,
                                              size_t height,
                                              const PlatformFeatures& platformFeatures) override
    {
        if (framebufferID == 0)
        {
            return nullptr; // FBO 0 doesn't support additional color attachments.
        }
        auto renderTarget =
            rcp(new PLSRenderTargetGL(framebufferID, width, height, platformFeatures));
        renderTarget->allocateCoverageBackingTextures();
        renderTarget->attachTexturesToCurrentFramebuffer();
        return renderTarget;
    }

    rcp<PLSRenderTargetGL> makeOffscreenRenderTarget(
        size_t width,
        size_t height,
        const PlatformFeatures& platformFeatures) override
    {
        auto renderTarget = rcp(new PLSRenderTargetGL(width, height, platformFeatures));
        renderTarget->allocateCoverageBackingTextures();
        renderTarget->attachTexturesToCurrentFramebuffer();
        return renderTarget;
    }

    void activatePixelLocalStorage(PLSRenderContextGL* context,
                                   const PLSRenderTargetGL* renderTarget,
                                   LoadAction loadAction,
                                   bool needsClipBuffer) override
    {
        assert(context->m_extensions.EXT_shader_framebuffer_fetch);

        glBindFramebuffer(GL_FRAMEBUFFER, renderTarget->drawFramebufferID());

        // Enable multiple render targets, with a draw buffer for each PLS plane.
        glDrawBuffers(4, kPLSDrawBuffers);

        // Instruct the driver not to load existing PLS plane contents into tiled memory, with the
        // exception of the color buffer after an intermediate flush.
        static_assert(kFramebufferPlaneIdx == 0);
        glInvalidateFramebuffer(GL_FRAMEBUFFER,
                                loadAction == LoadAction::clear ? 4 : 3,
                                loadAction == LoadAction::clear ? kPLSDrawBuffers
                                                                : kPLSDrawBuffers + 1);

        // Clear the PLS planes.
        constexpr static uint32_t kZero[4]{};
        if (loadAction == LoadAction::clear)
        {
            float clearColor4f[4];
            UnpackColorToRGBA32F(context->frameDescriptor().clearColor, clearColor4f);
            glClearBufferfv(GL_COLOR, kFramebufferPlaneIdx, clearColor4f);
        }
        glClearBufferuiv(GL_COLOR, kCoveragePlaneIdx, kZero);
        if (needsClipBuffer)
        {
            glClearBufferuiv(GL_COLOR, kClipPlaneIdx, kZero);
        }
    }

    void deactivatePixelLocalStorage(PLSRenderContextGL*) override
    {
        // Instruct the driver not to flush PLS contents from tiled memory, with the exception of
        // the color buffer.
        static_assert(kFramebufferPlaneIdx == 0);
        glInvalidateFramebuffer(GL_FRAMEBUFFER, 3, kPLSDrawBuffers + 1);

        // Don't restore glDrawBuffers. The client can assume we've changed it, if we have a wrapped
        // render target.
    }

    const char* shaderDefineName() const override { return GLSL_PLS_IMPL_FRAMEBUFFER_FETCH; }

    void onEnableRasterOrdering(bool rasterOrderingEnabled) override
    {
        if (!m_extensions.QCOM_shader_framebuffer_fetch_noncoherent)
        {
            return;
        }
        if (rasterOrderingEnabled)
        {
            glDisable(GL_FRAMEBUFFER_FETCH_NONCOHERENT_QCOM);
        }
        else
        {
            glEnable(GL_FRAMEBUFFER_FETCH_NONCOHERENT_QCOM);
        }
    }

    void onBarrier() override
    {
        if (!m_extensions.QCOM_shader_framebuffer_fetch_noncoherent)
        {
            return;
        }
        glFramebufferFetchBarrierQCOM();
    }

private:
    const GLExtensions m_extensions;
};

std::unique_ptr<PLSRenderContextGL::PLSImpl> PLSRenderContextGL::MakePLSImplFramebufferFetch(
    GLExtensions extensions)
{
    return std::make_unique<PLSImplFramebufferFetch>(extensions);
}
} // namespace rive::pls

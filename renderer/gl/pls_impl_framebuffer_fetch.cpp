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
        AttachRenderTargetTexturesToFramebuffer(renderTarget.get());
        return renderTarget;
    }

    rcp<PLSRenderTargetGL> makeOffscreenRenderTarget(
        size_t width,
        size_t height,
        const PlatformFeatures& platformFeatures) override
    {
        auto renderTarget = rcp(new PLSRenderTargetGL(width, height, platformFeatures));
        renderTarget->allocateCoverageBackingTextures();
        AttachRenderTargetTexturesToFramebuffer(renderTarget.get());
        return renderTarget;
    }

    static void AttachRenderTargetTexturesToFramebuffer(const PLSRenderTargetGL* renderTarget)
    {
        if (renderTarget->m_offscreenTextureID != 0)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                                   GL_COLOR_ATTACHMENT0 + kFramebufferPlaneIdx,
                                   GL_TEXTURE_2D,
                                   renderTarget->m_offscreenTextureID,
                                   0);
        }
        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0 + kCoveragePlaneIdx,
                               GL_TEXTURE_2D,
                               renderTarget->m_coverageTextureID,
                               0);
        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0 + kOriginalDstColorPlaneIdx,
                               GL_TEXTURE_2D,
                               renderTarget->m_originalDstColorTextureID,
                               0);
        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0 + kClipPlaneIdx,
                               GL_TEXTURE_2D,
                               renderTarget->m_clipTextureID,
                               0);
    }

    void activatePixelLocalStorage(PLSRenderContextGL* context,
                                   LoadAction loadAction,
                                   const ShaderFeatures& shaderFeatures,
                                   const DrawProgram& drawProgram) override
    {
        assert(context->m_extensions.EXT_shader_framebuffer_fetch);

        // Enable multiple render targets, with a draw buffer for each PLS plane.
        GLsizei numDrawBuffers = shaderFeatures.programFeatures.enablePathClipping ? 4 : 3;
        glDrawBuffers(numDrawBuffers, kPLSDrawBuffers);

        // Instruct the driver not to load existing PLS plane contents into tiled memory, with the
        // exception of the color buffer after an intermediate flush.
        static_assert(kFramebufferPlaneIdx == 0);
        glInvalidateFramebuffer(
            GL_FRAMEBUFFER,
            loadAction == LoadAction::clear ? numDrawBuffers : numDrawBuffers - 1,
            loadAction == LoadAction::clear ? kPLSDrawBuffers : kPLSDrawBuffers + 1);

        // Clear the PLS planes.
        constexpr static uint32_t kZero[4]{};
        if (loadAction == LoadAction::clear)
        {
            float clearColor4f[4];
            UnpackColorToRGBA32F(context->frameDescriptor().clearColor, clearColor4f);
            glClearBufferfv(GL_COLOR, kFramebufferPlaneIdx, clearColor4f);
        }
        glClearBufferuiv(GL_COLOR, kCoveragePlaneIdx, kZero);
        if (shaderFeatures.programFeatures.enablePathClipping)
        {
            glClearBufferuiv(GL_COLOR, kClipPlaneIdx, kZero);
        }

        context->bindDrawProgram(drawProgram);
    }

    void deactivatePixelLocalStorage(const ShaderFeatures& shaderFeatures) override
    {
        // Instruct the driver not to flush PLS contents from tiled memory, with the exception of
        // the color buffer.
        static_assert(kFramebufferPlaneIdx == 0);
        GLsizei numDrawBuffers = shaderFeatures.programFeatures.enablePathClipping ? 4 : 3;
        glInvalidateFramebuffer(GL_FRAMEBUFFER, numDrawBuffers - 1, kPLSDrawBuffers + 1);

        // Don't restore glDrawBuffers. The client can assume we've changed it, if we have a wrapped
        // render target.
    }

    const char* shaderDefineName() const override { return GLSL_PLS_IMPL_FRAMEBUFFER_FETCH; }
};

std::unique_ptr<PLSRenderContextGL::PLSImpl> PLSRenderContextGL::MakePLSImplFramebufferFetch()
{
    return std::make_unique<PLSImplFramebufferFetch>();
}
} // namespace rive::pls

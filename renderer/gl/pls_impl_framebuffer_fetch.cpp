/*
 * Copyright 2023 Rive
 */

#include "rive/pls/gl/pls_render_context_gl_impl.hpp"

#include "rive/pls/gl/pls_render_target_gl.hpp"
#include "shaders/constants.glsl"

#include "../out/obj/generated/glsl.exports.h"

namespace rive::pls
{
constexpr static GLenum kPLSDrawBuffers[4] = {GL_COLOR_ATTACHMENT0,
                                              GL_COLOR_ATTACHMENT1,
                                              GL_COLOR_ATTACHMENT2,
                                              GL_COLOR_ATTACHMENT3};

class PLSRenderContextGLImpl::PLSImplFramebufferFetch : public PLSRenderContextGLImpl::PLSImpl
{
public:
    PLSImplFramebufferFetch(const GLCapabilities& extensions) : m_capabilities(extensions) {}

    rcp<PLSRenderTargetGL> wrapGLRenderTarget(GLuint framebufferID,
                                              uint32_t width,
                                              uint32_t height,
                                              const PlatformFeatures& platformFeatures) override
    {
        if (framebufferID == 0)
        {
            return nullptr; // FBO 0 doesn't support additional color attachments.
        }
        auto renderTarget =
            rcp(new PLSRenderTargetGL(framebufferID, width, height, platformFeatures));
        renderTarget->allocateCoverageBackingTextures();
        renderTarget->attachCoverageTexturesToCurrentFramebuffer();
        return renderTarget;
    }

    rcp<PLSRenderTargetGL> makeOffscreenRenderTarget(
        uint32_t width,
        uint32_t height,
        PLSRenderTargetGL::TargetTextureOwnership targetTextureOwnership,
        const PlatformFeatures& platformFeatures) override
    {
        auto renderTarget =
            rcp(new PLSRenderTargetGL(width, height, targetTextureOwnership, platformFeatures));
        renderTarget->allocateCoverageBackingTextures();
        renderTarget->attachCoverageTexturesToCurrentFramebuffer();
        return renderTarget;
    }

    void activatePixelLocalStorage(PLSRenderContextGLImpl* impl,
                                   const FlushDescriptor& desc) override
    {
        assert(impl->m_capabilities.EXT_shader_framebuffer_fetch);

        auto renderTarget = static_cast<const PLSRenderTargetGL*>(desc.renderTarget);
        glBindFramebuffer(GL_FRAMEBUFFER, renderTarget->drawFramebufferID());
        renderTarget->reattachTargetTextureIfDifferent();

        // Enable multiple render targets, with a draw buffer for each PLS plane.
        glDrawBuffers((desc.combinedShaderFeatures & pls::ShaderFeatures::ENABLE_CLIPPING) ? 4 : 3,
                      kPLSDrawBuffers);

        // Instruct the driver not to load existing PLS plane contents into tiled memory, with the
        // exception of the color buffer after an intermediate flush.
        static_assert(FRAMEBUFFER_PLANE_IDX == 0);
        glInvalidateFramebuffer(GL_FRAMEBUFFER,
                                desc.loadAction == LoadAction::clear ? 4 : 3,
                                desc.loadAction == LoadAction::clear ? kPLSDrawBuffers
                                                                     : kPLSDrawBuffers + 1);

        // Clear the PLS planes.
        constexpr static uint32_t kZero[4]{};
        if (desc.loadAction == LoadAction::clear)
        {
            float clearColor4f[4];
            UnpackColorToRGBA32F(desc.clearColor, clearColor4f);
            glClearBufferfv(GL_COLOR, FRAMEBUFFER_PLANE_IDX, clearColor4f);
        }
        glClearBufferuiv(GL_COLOR, COVERAGE_PLANE_IDX, kZero);
        if (desc.combinedShaderFeatures & pls::ShaderFeatures::ENABLE_CLIPPING)
        {
            glClearBufferuiv(GL_COLOR, CLIP_PLANE_IDX, kZero);
        }
    }

    void deactivatePixelLocalStorage(PLSRenderContextGLImpl*) override
    {
        // Instruct the driver not to flush PLS contents from tiled memory, with the exception of
        // the color buffer.
        static_assert(FRAMEBUFFER_PLANE_IDX == 0);
        glInvalidateFramebuffer(GL_FRAMEBUFFER, 3, kPLSDrawBuffers + 1);

        // Don't restore glDrawBuffers. The client can assume we've changed it, if we have a wrapped
        // render target.
    }

    const char* shaderDefineName() const override { return GLSL_PLS_IMPL_FRAMEBUFFER_FETCH; }

    void onEnableRasterOrdering(bool rasterOrderingEnabled) override
    {
        if (!m_capabilities.QCOM_shader_framebuffer_fetch_noncoherent)
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
        if (!m_capabilities.QCOM_shader_framebuffer_fetch_noncoherent)
        {
            return;
        }
        glFramebufferFetchBarrierQCOM();
    }

private:
    const GLCapabilities m_capabilities;
};

std::unique_ptr<PLSRenderContextGLImpl::PLSImpl> PLSRenderContextGLImpl::
    MakePLSImplFramebufferFetch(const GLCapabilities& extensions)
{
    return std::make_unique<PLSImplFramebufferFetch>(extensions);
}
} // namespace rive::pls

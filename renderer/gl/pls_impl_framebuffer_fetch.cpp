/*
 * Copyright 2023 Rive
 */

#include "rive/pls/gl/pls_render_context_gl_impl.hpp"

#include "gl_utils.hpp"
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

    void activatePixelLocalStorage(PLSRenderContextGLImpl* impl,
                                   const FlushDescriptor& desc) override
    {
        assert(impl->m_capabilities.EXT_shader_framebuffer_fetch);

        auto renderTarget = static_cast<PLSRenderTargetGL*>(desc.renderTarget);
        renderTarget->allocateInternalPLSTextures(desc.interlockMode);

        if (auto framebufferRenderTarget = lite_rtti_cast<FramebufferRenderTargetGL*>(renderTarget))
        {
            // We're targeting an external FBO directly. Allocate and attach an offscreen target
            // texture since we can't modify their attachments.
            framebufferRenderTarget->allocateOffscreenTargetTexture();
            if (desc.loadAction == LoadAction::preserveRenderTarget)
            {
                // Copy the framebuffer's contents to our offscreen texture.
                framebufferRenderTarget->bindExternalFramebuffer(GL_READ_FRAMEBUFFER);
                framebufferRenderTarget->bindInternalFramebuffer(GL_DRAW_FRAMEBUFFER, 1);
                glutils::BlitFramebuffer(framebufferRenderTarget->bounds(),
                                         framebufferRenderTarget->height());
            }
        }

        renderTarget->bindInternalFramebuffer(GL_DRAW_FRAMEBUFFER, 4);

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

    void deactivatePixelLocalStorage(PLSRenderContextGLImpl*, const FlushDescriptor& desc) override
    {
        // Instruct the driver not to flush PLS contents from tiled memory, with the exception of
        // the color buffer.
        static_assert(FRAMEBUFFER_PLANE_IDX == 0);
        glInvalidateFramebuffer(GL_FRAMEBUFFER, 3, kPLSDrawBuffers + 1);

        if (auto framebufferRenderTarget = lite_rtti_cast<FramebufferRenderTargetGL*>(
                static_cast<PLSRenderTargetGL*>(desc.renderTarget)))
        {
            // We rendered to an offscreen texture. Copy back to the external framebuffer.
            framebufferRenderTarget->bindInternalFramebuffer(GL_READ_FRAMEBUFFER);
            framebufferRenderTarget->bindExternalFramebuffer(GL_DRAW_FRAMEBUFFER);
            glutils::BlitFramebuffer(framebufferRenderTarget->bounds(),
                                     framebufferRenderTarget->height());
        }
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

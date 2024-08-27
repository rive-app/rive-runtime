/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/gl/render_context_gl_impl.hpp"

#include "rive/renderer/gl/gl_utils.hpp"
#include "rive/renderer/gl/render_target_gl.hpp"
#include "shaders/constants.glsl"

#include "generated/shaders/glsl.exports.h"

namespace rive::gpu
{
using DrawBufferMask = PLSRenderTargetGL::DrawBufferMask;

constexpr static GLenum kPLSDrawBuffers[4] = {GL_COLOR_ATTACHMENT0,
                                              GL_COLOR_ATTACHMENT1,
                                              GL_COLOR_ATTACHMENT2,
                                              GL_COLOR_ATTACHMENT3};

class PLSRenderContextGLImpl::PLSImplFramebufferFetch : public PLSRenderContextGLImpl::PLSImpl
{
public:
    PLSImplFramebufferFetch(const GLCapabilities& extensions) : m_capabilities(extensions) {}

    bool supportsRasterOrdering(const GLCapabilities&) const override { return true; }

    void activatePixelLocalStorage(PLSRenderContextGLImpl* plsContextImpl,
                                   const FlushDescriptor& desc) override
    {
        assert(plsContextImpl->m_capabilities.EXT_shader_framebuffer_fetch);

        auto renderTarget = static_cast<PLSRenderTargetGL*>(desc.renderTarget);
        renderTarget->allocateInternalPLSTextures(desc.interlockMode);

        if (auto framebufferRenderTarget = lite_rtti_cast<FramebufferRenderTargetGL*>(renderTarget))
        {
            // We're targeting an external FBO directly. Allocate and attach an offscreen target
            // texture since we can't modify their attachments.
            framebufferRenderTarget->allocateOffscreenTargetTexture();
            if (desc.colorLoadAction == LoadAction::preserveRenderTarget)
            {
                // Copy the framebuffer's contents to our offscreen texture.
                framebufferRenderTarget->bindDestinationFramebuffer(GL_READ_FRAMEBUFFER);
                framebufferRenderTarget->bindInternalFramebuffer(GL_DRAW_FRAMEBUFFER,
                                                                 DrawBufferMask::color);
                glutils::BlitFramebuffer(desc.renderTargetUpdateBounds,
                                         framebufferRenderTarget->height());
            }
        }

        GLuint coverageClear[4]{desc.coverageClearValue};
        auto fbFetchBuffers = DrawBufferMask::color;
        if (desc.combinedShaderFeatures & gpu::ShaderFeatures::ENABLE_CLIPPING)
        {
            fbFetchBuffers |= DrawBufferMask::clip;
        }
        if (desc.interlockMode == gpu::InterlockMode::rasterOrdering)
        {
            fbFetchBuffers |= DrawBufferMask::coverage | DrawBufferMask::scratchColor;
        }
        else
        {
            // Clear and bind the coverage buffer now, since it won't be enabled on the framebuffer.
            renderTarget->bindInternalFramebuffer(GL_DRAW_FRAMEBUFFER, DrawBufferMask::coverage);
            glClearBufferuiv(GL_COLOR, COVERAGE_PLANE_IDX, coverageClear);
            renderTarget->bindAsImageTextures(DrawBufferMask::coverage);
        }
        renderTarget->bindInternalFramebuffer(GL_DRAW_FRAMEBUFFER, fbFetchBuffers);

        // Instruct the driver not to load existing PLS plane contents into tiled memory, with the
        // exception of the color buffer if it's preserved.
        static_assert(COLOR_PLANE_IDX == 0);
        glInvalidateFramebuffer(GL_FRAMEBUFFER,
                                desc.colorLoadAction == LoadAction::preserveRenderTarget ? 3 : 4,
                                desc.colorLoadAction == LoadAction::preserveRenderTarget
                                    ? kPLSDrawBuffers + 1
                                    : kPLSDrawBuffers);

        // Clear the PLS planes.
        if (desc.colorLoadAction == LoadAction::clear)
        {
            float clearColor4f[4];
            UnpackColorToRGBA32F(desc.clearColor, clearColor4f);
            glClearBufferfv(GL_COLOR, COLOR_PLANE_IDX, clearColor4f);
        }
        if (desc.combinedShaderFeatures & gpu::ShaderFeatures::ENABLE_CLIPPING)
        {
            constexpr static uint32_t kZero[4]{};
            glClearBufferuiv(GL_COLOR, CLIP_PLANE_IDX, kZero);
        }
        if (fbFetchBuffers & DrawBufferMask::coverage)
        {
            glClearBufferuiv(GL_COLOR, COVERAGE_PLANE_IDX, coverageClear);
        }

        if (desc.interlockMode == gpu::InterlockMode::atomics &&
            !(desc.combinedShaderFeatures & gpu::ShaderFeatures::ENABLE_CLIP_RECT))
        {
            plsContextImpl->state()->setBlendEquation(BlendMode::srcOver);
        }

        if (desc.interlockMode == gpu::InterlockMode::atomics)
        {
            glMemoryBarrierByRegion(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        }
    }

    void deactivatePixelLocalStorage(PLSRenderContextGLImpl*, const FlushDescriptor& desc) override
    {
        if (desc.interlockMode == gpu::InterlockMode::atomics)
        {
            glMemoryBarrierByRegion(GL_ALL_BARRIER_BITS);
        }

        // Instruct the driver not to flush PLS contents from tiled memory, with the exception of
        // the color buffer.
        static_assert(COLOR_PLANE_IDX == 0);
        glInvalidateFramebuffer(GL_FRAMEBUFFER, 3, kPLSDrawBuffers + 1);

        if (auto framebufferRenderTarget = lite_rtti_cast<FramebufferRenderTargetGL*>(
                static_cast<PLSRenderTargetGL*>(desc.renderTarget)))
        {
            // We rendered to an offscreen texture. Copy back to the external framebuffer.
            framebufferRenderTarget->bindInternalFramebuffer(GL_READ_FRAMEBUFFER,
                                                             DrawBufferMask::color);
            framebufferRenderTarget->bindDestinationFramebuffer(GL_DRAW_FRAMEBUFFER);
            glutils::BlitFramebuffer(desc.renderTargetUpdateBounds,
                                     framebufferRenderTarget->height());
        }
    }

    void pushShaderDefines(gpu::InterlockMode interlockMode,
                           std::vector<const char*>* defines) const override
    {
        defines->push_back(GLSL_PLS_IMPL_FRAMEBUFFER_FETCH);
        if (interlockMode == gpu::InterlockMode::atomics)
        {
            defines->push_back(GLSL_USING_PLS_STORAGE_TEXTURES);
        }
    }

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

    void onBarrier(const gpu::FlushDescriptor& desc) override
    {
        if (m_capabilities.QCOM_shader_framebuffer_fetch_noncoherent)
        {
            glFramebufferFetchBarrierQCOM();
        }
        if (desc.interlockMode == gpu::InterlockMode::atomics)
        {
            glMemoryBarrierByRegion(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        }
    }

private:
    const GLCapabilities m_capabilities;
};

std::unique_ptr<PLSRenderContextGLImpl::PLSImpl> PLSRenderContextGLImpl::
    MakePLSImplFramebufferFetch(const GLCapabilities& extensions)
{
    return std::make_unique<PLSImplFramebufferFetch>(extensions);
}
} // namespace rive::gpu

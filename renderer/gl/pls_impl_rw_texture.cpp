/*
 * Copyright 2023 Rive
 */

#include "rive/pls/gl/pls_render_context_gl_impl.hpp"

#include "rive/pls/gl/pls_render_target_gl.hpp"
#include "shaders/constants.glsl"
#include "gl_utils.hpp"

#include "shaders/out/generated/glsl.exports.h"

namespace rive::pls
{
class PLSRenderContextGLImpl::PLSImplRWTexture : public PLSRenderContextGLImpl::PLSImpl
{
    bool supportsRasterOrdering(const GLCapabilities& capabilities) const override
    {
        return capabilities.ARB_fragment_shader_interlock ||
               capabilities.INTEL_fragment_shader_ordering;
    }

    void activatePixelLocalStorage(PLSRenderContextGLImpl* plsContextImpl,
                                   const FlushDescriptor& desc) override
    {
        auto renderTarget = static_cast<PLSRenderTargetGL*>(desc.renderTarget);
        renderTarget->allocateInternalPLSTextures(desc.interlockMode);

        bool renderDirectToRasterPipeline =
            pls::ShadersEmitColorToRasterPipeline(desc.interlockMode, desc.combinedShaderFeatures);
        if (renderDirectToRasterPipeline)
        {
            plsContextImpl->state()->setBlendEquation(BlendMode::srcOver);
        }
        else if (auto framebufferRenderTarget =
                     lite_rtti_cast<FramebufferRenderTargetGL*>(renderTarget))
        {
            // We're targeting an external FBO but can't render to it directly. Make sure to
            // allocate and attach an offscreen target texture.
            framebufferRenderTarget->allocateOffscreenTargetTexture();
            if (desc.colorLoadAction == pls::LoadAction::preserveRenderTarget)
            {
                // Copy the framebuffer's contents to our offscreen texture.
                framebufferRenderTarget->bindDestinationFramebuffer(GL_READ_FRAMEBUFFER);
                framebufferRenderTarget->bindInternalFramebuffer(GL_DRAW_FRAMEBUFFER, 1);
                glutils::BlitFramebuffer(desc.renderTargetUpdateBounds, renderTarget->height());
            }
        }

        // Clear the necessary textures.
        renderTarget->bindInternalFramebuffer(GL_FRAMEBUFFER, 4);
        if (desc.colorLoadAction == pls::LoadAction::clear)
        {
            float clearColor4f[4];
            UnpackColorToRGBA32F(desc.clearColor, clearColor4f);
            glClearBufferfv(GL_COLOR, FRAMEBUFFER_PLANE_IDX, clearColor4f);
        }
        {
            GLuint coverageClear[4]{desc.coverageClearValue};
            glClearBufferuiv(GL_COLOR, COVERAGE_PLANE_IDX, coverageClear);
        }
        if (desc.combinedShaderFeatures & pls::ShaderFeatures::ENABLE_CLIPPING)
        {
            constexpr static GLuint kZeroClear[4]{};
            glClearBufferuiv(GL_COLOR, CLIP_PLANE_IDX, kZeroClear);
        }

        // Bind the framebuffer we will render to.
        if (!renderDirectToRasterPipeline)
        {
            renderTarget->bindHeadlessFramebuffer(plsContextImpl->m_capabilities);
        }
        else
        {
            renderTarget->bindDestinationFramebuffer(GL_FRAMEBUFFER);
        }

        renderTarget->bindAsImageTextures();

        glMemoryBarrierByRegion(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        assert(m_didCoalescedPLSResolveAndTransfer == false);
    }

    bool supportsCoalescedPLSResolveAndTransfer(
        const PLSRenderTargetGL* renderTarget) const override
    {
        return lite_rtti_cast<const FramebufferRenderTargetGL*>(renderTarget) != nullptr;
    }

    void setupCoalescedPLSResolveAndTransfer(PLSRenderTargetGL* renderTarget) override
    {
        // Bind the external target framebuffer for rendering the PLS resolve operation.
        assert(lite_rtti_cast<FramebufferRenderTargetGL*>(renderTarget) != nullptr);
        renderTarget->bindDestinationFramebuffer(GL_FRAMEBUFFER);
        m_didCoalescedPLSResolveAndTransfer = true;
    }

    void deactivatePixelLocalStorage(PLSRenderContextGLImpl*, const FlushDescriptor& desc) override
    {
        glMemoryBarrierByRegion(GL_ALL_BARRIER_BITS);

        if (pls::ShadersEmitColorToRasterPipeline(desc.interlockMode, desc.combinedShaderFeatures))
        {
            assert(!m_didCoalescedPLSResolveAndTransfer);
            return;
        }

        if (m_didCoalescedPLSResolveAndTransfer)
        {
            m_didCoalescedPLSResolveAndTransfer = false;
            return;
        }

        if (auto framebufferRenderTarget = lite_rtti_cast<FramebufferRenderTargetGL*>(
                static_cast<PLSRenderTargetGL*>(desc.renderTarget)))
        {
            // We rendered to an offscreen texture. Copy back to the external target framebuffer.
            framebufferRenderTarget->bindInternalFramebuffer(GL_READ_FRAMEBUFFER, 1);
            framebufferRenderTarget->bindDestinationFramebuffer(GL_DRAW_FRAMEBUFFER);
            glutils::BlitFramebuffer(desc.renderTargetUpdateBounds,
                                     framebufferRenderTarget->height());
        }
    }

    const char* shaderDefineName() const override { return GLSL_PLS_IMPL_RW_TEXTURE; }

    void onBarrier() override
    {
        return glMemoryBarrierByRegion(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    bool m_didCoalescedPLSResolveAndTransfer = false;
};

std::unique_ptr<PLSRenderContextGLImpl::PLSImpl> PLSRenderContextGLImpl::MakePLSImplRWTexture()
{
    return std::make_unique<PLSImplRWTexture>();
}
} // namespace rive::pls

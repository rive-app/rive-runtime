/*
 * Copyright 2023 Rive
 */

#include "rive/pls/gl/pls_render_context_gl_impl.hpp"

#include "shaders/constants.glsl"
#include "gl_utils.hpp"

#include "../out/obj/generated/glsl.exports.h"

namespace rive::pls
{
class PLSRenderContextGLImpl::PLSImplRWTexture : public PLSRenderContextGLImpl::PLSImpl
{
    void activatePixelLocalStorage(PLSRenderContextGLImpl* plsContextImpl,
                                   const FlushDescriptor& desc) override
    {
        auto renderTarget = static_cast<PLSRenderTargetGL*>(desc.renderTarget);
        renderTarget->allocateInternalPLSTextures(desc.interlockMode);

        auto framebufferRenderTarget = lite_rtti_cast<FramebufferRenderTargetGL*>(renderTarget);
        if (framebufferRenderTarget != nullptr)
        {
            // We're targeting an external FBO directly. Make sure to allocate and attach an
            // offscreen target texture.
            framebufferRenderTarget->allocateOffscreenTargetTexture();
            if (desc.loadAction == LoadAction::preserveRenderTarget)
            {
                // Copy the framebuffer's contents to our offscreen texture.
                framebufferRenderTarget->bindExternalFramebuffer(GL_READ_FRAMEBUFFER);
                framebufferRenderTarget->bindInternalFramebuffer(GL_DRAW_FRAMEBUFFER, 1);
                glutils::BlitFramebuffer(renderTarget->bounds(), renderTarget->height());
            }
        }

        // Clear the necessary textures.
        renderTarget->bindInternalFramebuffer(GL_FRAMEBUFFER, 4);
        if (desc.canSkipColorClear())
        {
            // We can accomplish a clear of the color buffer while the shader resolves coverage,
            // instead of calling glClear. The fill for pathID=0 is automatically configured to be
            // a solid fill matching the clear color, so we just have to clear the coverage buffer
            // to solid coverage. This ensures the clear color gets written out fully opaque.
            constexpr static GLuint kCoverageOne[4]{static_cast<GLuint>(FIXED_COVERAGE_ONE)};
            glClearBufferuiv(GL_COLOR, COVERAGE_PLANE_IDX, kCoverageOne);
        }
        else
        {
            if (desc.loadAction == LoadAction::clear)
            {
                float clearColor4f[4];
                UnpackColorToRGBA32F(desc.clearColor, clearColor4f);
                glClearBufferfv(GL_COLOR, FRAMEBUFFER_PLANE_IDX, clearColor4f);
            }
            // Clear the coverage buffer to pathID=0, and with a value that means "zero coverage" in
            // atomic mode. In non-atomic mode all we need is for pathID to be zero. In atomic mode,
            // the additional "zero coverage" value makes sure nothing gets written out at this
            // pixel when resolving.
            constexpr static GLuint kCoverageZero[4]{static_cast<GLuint>(FIXED_COVERAGE_ZERO)};
            glClearBufferuiv(GL_COLOR, COVERAGE_PLANE_IDX, kCoverageZero);
        }
        if (desc.combinedShaderFeatures & pls::ShaderFeatures::ENABLE_CLIPPING)
        {
            constexpr static GLuint kZero[4]{};
            glClearBufferuiv(GL_COLOR, CLIP_PLANE_IDX, kZero);
        }

        renderTarget->bindHeadlessFramebuffer(plsContextImpl->m_capabilities);
        renderTarget->bindAsImageTextures();
        glMemoryBarrierByRegion(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    void deactivatePixelLocalStorage(PLSRenderContextGLImpl*, const FlushDescriptor& desc) override
    {
        glMemoryBarrierByRegion(GL_ALL_BARRIER_BITS);

        if (auto framebufferRenderTarget = lite_rtti_cast<FramebufferRenderTargetGL*>(
                static_cast<PLSRenderTargetGL*>(desc.renderTarget)))
        {
            // We rendered to an offscreen texture. Copy back to the external target framebuffer.
            framebufferRenderTarget->bindInternalFramebuffer(GL_READ_FRAMEBUFFER);
            framebufferRenderTarget->bindExternalFramebuffer(GL_DRAW_FRAMEBUFFER);
            glutils::BlitFramebuffer(framebufferRenderTarget->bounds(),
                                     framebufferRenderTarget->height());
        }
    }

    const char* shaderDefineName() const override { return GLSL_PLS_IMPL_RW_TEXTURE; }

    void onBarrier() override
    {
        return glMemoryBarrierByRegion(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }
};

std::unique_ptr<PLSRenderContextGLImpl::PLSImpl> PLSRenderContextGLImpl::MakePLSImplRWTexture()
{
    return std::make_unique<PLSImplRWTexture>();
}
} // namespace rive::pls

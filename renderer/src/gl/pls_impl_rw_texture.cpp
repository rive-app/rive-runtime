/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/gl/render_context_gl_impl.hpp"

#include "rive/renderer/gl/render_target_gl.hpp"
#include "shaders/constants.glsl"
#include "rive/renderer/gl/gl_utils.hpp"

#include "generated/shaders/glsl.exports.h"

namespace rive::gpu
{
using DrawBufferMask = RenderTargetGL::DrawBufferMask;

static bool needs_coalesced_atomic_resolve_and_transfer(
    const gpu::FlushDescriptor& desc)
{
    assert(desc.interlockMode == gpu::InterlockMode::atomics);
    return (desc.combinedShaderFeatures &
            ShaderFeatures::ENABLE_ADVANCED_BLEND) &&
           lite_rtti_cast<FramebufferRenderTargetGL*>(
               static_cast<RenderTargetGL*>(desc.renderTarget)) != nullptr;
}

class RenderContextGLImpl::PLSImplRWTexture
    : public RenderContextGLImpl::PixelLocalStorageImpl
{
    bool supportsRasterOrdering(
        const GLCapabilities& capabilities) const override
    {
        return capabilities.ARB_fragment_shader_interlock ||
               capabilities.INTEL_fragment_shader_ordering;
    }

    bool supportsFragmentShaderAtomics(
        const GLCapabilities& capabilities) const override
    {
        return true;
    }

    void activatePixelLocalStorage(RenderContextGLImpl* renderContextImpl,
                                   const FlushDescriptor& desc) override
    {
        auto renderTarget = static_cast<RenderTargetGL*>(desc.renderTarget);
        renderTarget->allocateInternalPLSTextures(desc.interlockMode);

        if (!desc.atomicFixedFunctionColorOutput)
        {
            if (auto framebufferRenderTarget =
                    lite_rtti_cast<FramebufferRenderTargetGL*>(renderTarget))
            {
                // We're targeting an external FBO but can't render to it
                // directly. Make sure to allocate and attach an offscreen
                // target texture.
                framebufferRenderTarget->allocateOffscreenTargetTexture();
                if (desc.colorLoadAction ==
                    gpu::LoadAction::preserveRenderTarget)
                {
                    // Copy the framebuffer's contents to our offscreen texture.
                    framebufferRenderTarget->bindDestinationFramebuffer(
                        GL_READ_FRAMEBUFFER);
                    framebufferRenderTarget->bindInternalFramebuffer(
                        GL_DRAW_FRAMEBUFFER,
                        DrawBufferMask::color);
                    glutils::BlitFramebuffer(desc.renderTargetUpdateBounds,
                                             renderTarget->height());
                }
            }
        }

        // Clear the necessary textures.
        auto rwTexBuffers = DrawBufferMask::coverage;
        if (desc.interlockMode == gpu::InterlockMode::rasterOrdering)
        {
            rwTexBuffers |=
                DrawBufferMask::color | DrawBufferMask::scratchColor;
        }
        else if (desc.combinedShaderFeatures &
                 ShaderFeatures::ENABLE_ADVANCED_BLEND)
        {
            rwTexBuffers |= DrawBufferMask::color;
        }
        if (desc.combinedShaderFeatures & gpu::ShaderFeatures::ENABLE_CLIPPING)
        {
            rwTexBuffers |= DrawBufferMask::clip;
        }
        renderTarget->bindInternalFramebuffer(GL_FRAMEBUFFER, rwTexBuffers);
        if (desc.colorLoadAction == gpu::LoadAction::clear &&
            (rwTexBuffers & DrawBufferMask::color))
        {
            // If the color buffer is not a storage texture, we will clear it
            // once the main framebuffer gets bound.
            float clearColor4f[4];
            UnpackColorToRGBA32FPremul(desc.colorClearValue, clearColor4f);
            glClearBufferfv(GL_COLOR, COLOR_PLANE_IDX, clearColor4f);
        }
        if (desc.combinedShaderFeatures & gpu::ShaderFeatures::ENABLE_CLIPPING)
        {
            constexpr static GLuint kZeroClear[4]{};
            glClearBufferuiv(GL_COLOR, CLIP_PLANE_IDX, kZeroClear);
        }
        {
            GLuint coverageClear[4]{desc.coverageClearValue};
            glClearBufferuiv(GL_COLOR, COVERAGE_PLANE_IDX, coverageClear);
        }

        switch (desc.interlockMode)
        {
            case gpu::InterlockMode::rasterOrdering:
                // rasterOrdering mode renders by storing to an image texture.
                // Bind a framebuffer with no color attachments.
                renderTarget->bindHeadlessFramebuffer(
                    renderContextImpl->m_capabilities);
                break;
            case gpu::InterlockMode::atomics:
                renderTarget->bindDestinationFramebuffer(GL_FRAMEBUFFER);
                if (desc.colorLoadAction == gpu::LoadAction::clear &&
                    !(rwTexBuffers & DrawBufferMask::color))
                {
                    // We're rendering directly to the main framebuffer. Clear
                    // it now.
                    float cc[4];
                    UnpackColorToRGBA32FPremul(desc.colorClearValue, cc);
                    glClearColor(cc[0], cc[1], cc[2], cc[3]);
                    glClear(GL_COLOR_BUFFER_BIT);
                }
                break;
            default:
                RIVE_UNREACHABLE();
        }

        renderTarget->bindAsImageTextures(rwTexBuffers);

        glMemoryBarrierByRegion(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    gpu::ShaderMiscFlags shaderMiscFlags(const gpu::FlushDescriptor& desc,
                                         gpu::DrawType drawType) const final
    {
        auto flags = gpu::ShaderMiscFlags::none;
        if (desc.interlockMode == gpu::InterlockMode::atomics)
        {
            if (desc.atomicFixedFunctionColorOutput)
            {
                flags |= gpu::ShaderMiscFlags::fixedFunctionColorOutput;
            }
            if (drawType == gpu::DrawType::atomicResolve &&
                needs_coalesced_atomic_resolve_and_transfer(desc))
            {
                flags |= gpu::ShaderMiscFlags::coalescedResolveAndTransfer;
            }
        }
        return flags;
    }

    void deactivatePixelLocalStorage(RenderContextGLImpl*,
                                     const FlushDescriptor& desc) override
    {
        glMemoryBarrierByRegion(GL_ALL_BARRIER_BITS);

        // atomic mode never needs to copy anything here because it transfers
        // the offscreen texture during resolve.
        if (desc.interlockMode == gpu::InterlockMode::rasterOrdering)
        {
            if (auto framebufferRenderTarget =
                    lite_rtti_cast<FramebufferRenderTargetGL*>(
                        static_cast<RenderTargetGL*>(desc.renderTarget)))
            {
                // We rendered to an offscreen texture. Copy back to the
                // external target framebuffer.
                framebufferRenderTarget->bindInternalFramebuffer(
                    GL_READ_FRAMEBUFFER,
                    DrawBufferMask::color);
                framebufferRenderTarget->bindDestinationFramebuffer(
                    GL_DRAW_FRAMEBUFFER);
                glutils::BlitFramebuffer(desc.renderTargetUpdateBounds,
                                         framebufferRenderTarget->height());
            }
        }
    }

    void pushShaderDefines(gpu::InterlockMode,
                           std::vector<const char*>* defines) const override
    {
        defines->push_back(GLSL_PLS_IMPL_STORAGE_TEXTURE);
        defines->push_back(GLSL_USING_PLS_STORAGE_TEXTURES);
    }

    void onBarrier(const gpu::FlushDescriptor&) override
    {
        return glMemoryBarrierByRegion(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }
};

std::unique_ptr<RenderContextGLImpl::PixelLocalStorageImpl>
RenderContextGLImpl::MakePLSImplRWTexture()
{
    return std::make_unique<PLSImplRWTexture>();
}
} // namespace rive::gpu

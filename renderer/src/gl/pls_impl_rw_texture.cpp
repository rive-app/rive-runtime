/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/gl/render_context_gl_impl.hpp"

#include "rive/renderer/gl/render_target_gl.hpp"
#include "shaders/constants.glsl"
#include "rive/renderer/gl/gl_utils.hpp"

#include "generated/shaders/glsl.glsl.exports.h"

#include <numeric>

namespace rive::gpu
{
static bool needs_coalesced_atomic_resolve_and_transfer(
    const gpu::FlushDescriptor& desc)
{
    assert(desc.interlockMode == gpu::InterlockMode::atomics);
    return !desc.fixedFunctionColorOutput &&
           lite_rtti_cast<FramebufferRenderTargetGL*>(
               static_cast<RenderTargetGL*>(desc.renderTarget)) != nullptr;
}

class RenderContextGLImpl::PLSImplRWTexture
    : public RenderContextGLImpl::PixelLocalStorageImpl
{
    void init(rcp<GLState>) override
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_plsClearFBO);
        std::array<GLenum, PLS_CLEAR_BUFFER_COUNT> plsClearBuffers;
        std::iota(plsClearBuffers.begin(),
                  plsClearBuffers.end(),
                  GL_COLOR_ATTACHMENT0);
        glDrawBuffers(plsClearBuffers.size(), plsClearBuffers.data());
    }

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

    void resizeTransientPLSBacking(uint32_t width,
                                   uint32_t height,
                                   uint32_t depth) override
    {
        assert(depth <= PLS_TRANSIENT_BACKING_MAX_DEPTH);
        if (width == 0 || height == 0 || depth == 0)
        {
            m_plsTransientBackingTexture = glutils::Texture::Zero();
        }
        else
        {
            m_plsTransientBackingTexture = glutils::Texture();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D_ARRAY, m_plsTransientBackingTexture);
            glTexStorage3D(GL_TEXTURE_2D_ARRAY,
                           1,
                           GL_R32UI,
                           width,
                           height,
                           depth);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, m_plsClearFBO);
        for (uint32_t i = 0; i < PLS_TRANSIENT_BACKING_MAX_DEPTH; ++i)
        {
            glFramebufferTextureLayer(
                GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0 + PLS_TRANSIENT_BACKING_CLEAR_IDX + i,
                (i < depth) ? m_plsTransientBackingTexture : 0,
                0,
                i);
        }
        static_assert(CLIP_PLANE_IDX == 1);
        static_assert(SCRATCH_COLOR_PLANE_IDX == 2);
        static_assert(COVERAGE_PLANE_IDX == 3);
    }

    void resizeAtomicCoverageBacking(uint32_t width, uint32_t height) override
    {
        if (width == 0 || height == 0)
        {
            m_atomicCoverageTexture = glutils::Texture::Zero();
        }
        else
        {
            m_atomicCoverageTexture = glutils::Texture();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_atomicCoverageTexture);
            glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, width, height);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, m_plsClearFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0 +
                                   PLS_ATOMIC_COVERAGE_CLEAR_IDX,
                               GL_TEXTURE_2D,
                               m_atomicCoverageTexture,
                               0);
    }

    void activatePixelLocalStorage(RenderContextGLImpl* renderContextImpl,
                                   const FlushDescriptor& desc) override
    {
        auto renderTarget = static_cast<RenderTargetGL*>(desc.renderTarget);

        // Bind and initialize the PLS backing textures.
        renderContextImpl->state()->setPipelineState(
            gpu::COLOR_ONLY_PIPELINE_STATE);
        renderContextImpl->state()->setScissor(desc.renderTargetUpdateBounds,
                                               renderTarget->height());

        if (!desc.fixedFunctionColorOutput)
        {
            if (desc.colorLoadAction == gpu::LoadAction::preserveRenderTarget)
            {
                if (auto framebufferRenderTarget =
                        lite_rtti_cast<FramebufferRenderTargetGL*>(
                            renderTarget))
                {
                    // We're targeting an external FBO but need to render to a
                    // texture. Since we also need to preserve the contents,
                    // blit the target framebuffer into the offscreen texture.
                    framebufferRenderTarget->bindDestinationFramebuffer(
                        GL_READ_FRAMEBUFFER);
                    framebufferRenderTarget->bindTextureFramebuffer(
                        GL_DRAW_FRAMEBUFFER);
                    glutils::BlitFramebuffer(desc.renderTargetUpdateBounds,
                                             renderTarget->height());
                }
            }
            // If the color buffer is *not* a storage texture, we will clear it
            // once the main framebuffer gets bound.
            if (desc.colorLoadAction == gpu::LoadAction::clear)
            {
                float clearColor4f[4];
                UnpackColorToRGBA32FPremul(desc.colorClearValue, clearColor4f);
                renderTarget->bindTextureFramebuffer(GL_FRAMEBUFFER);
                glClearBufferfv(GL_COLOR, COLOR_PLANE_IDX, clearColor4f);
            }
            glBindImageTexture(COLOR_PLANE_IDX,
                               renderTarget->renderTexture(),
                               0,
                               GL_FALSE,
                               0,
                               GL_READ_WRITE,
                               GL_RGBA8);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, m_plsClearFBO);
        uint32_t nextTransientLayer = 0;
        {
            GLuint coverageClear[4]{desc.coverageClearValue};
            if (desc.interlockMode == gpu::InterlockMode::rasterOrdering)
            {
                glClearBufferuiv(GL_COLOR, nextTransientLayer, coverageClear);
                glBindImageTexture(COVERAGE_PLANE_IDX,
                                   m_plsTransientBackingTexture,
                                   0,
                                   GL_FALSE,
                                   nextTransientLayer,
                                   GL_READ_WRITE,
                                   GL_R32UI);
                ++nextTransientLayer;
            }
            else
            {
                assert(desc.interlockMode == gpu::InterlockMode::atomics);
                glClearBufferuiv(GL_COLOR,
                                 PLS_ATOMIC_COVERAGE_CLEAR_IDX,
                                 coverageClear);
                glBindImageTexture(COVERAGE_PLANE_IDX,
                                   m_atomicCoverageTexture,
                                   0,
                                   GL_FALSE,
                                   0,
                                   GL_READ_WRITE,
                                   GL_R32UI);
            }
        }

        if (desc.combinedShaderFeatures & gpu::ShaderFeatures::ENABLE_CLIPPING)
        {
            constexpr static GLuint kZeroClear[4]{};
            glClearBufferuiv(GL_COLOR, nextTransientLayer, kZeroClear);
            glBindImageTexture(CLIP_PLANE_IDX,
                               m_plsTransientBackingTexture,
                               0,
                               GL_FALSE,
                               nextTransientLayer,
                               GL_READ_WRITE,
                               GL_R32UI);
            ++nextTransientLayer;
        }

        if (desc.interlockMode == gpu::InterlockMode::rasterOrdering)
        {
            glBindImageTexture(SCRATCH_COLOR_PLANE_IDX,
                               m_plsTransientBackingTexture,
                               0,
                               GL_FALSE,
                               nextTransientLayer,
                               GL_READ_WRITE,
                               GL_RGBA8);
            ++nextTransientLayer;
        }
        assert(nextTransientLayer <= PLS_TRANSIENT_BACKING_MAX_DEPTH);

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
                if (desc.fixedFunctionColorOutput &&
                    desc.colorLoadAction == gpu::LoadAction::clear)
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

        glMemoryBarrierByRegion(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    gpu::ShaderMiscFlags shaderMiscFlags(const gpu::FlushDescriptor& desc,
                                         gpu::DrawType drawType) const final
    {
        auto flags = gpu::ShaderMiscFlags::none;
        if (desc.interlockMode == gpu::InterlockMode::atomics)
        {
            if (desc.fixedFunctionColorOutput)
            {
                flags |= gpu::ShaderMiscFlags::fixedFunctionColorOutput;
            }
            if (drawType == gpu::DrawType::renderPassResolve &&
                needs_coalesced_atomic_resolve_and_transfer(desc))
            {
                flags |= gpu::ShaderMiscFlags::coalescedResolveAndTransfer;
            }
        }
        return flags;
    }

    void deactivatePixelLocalStorage(RenderContextGLImpl* renderContextImpl,
                                     const FlushDescriptor& desc) override
    {
        glMemoryBarrierByRegion(GL_ALL_BARRIER_BITS);

        // atomic mode never needs to copy anything here because it transfers
        // the offscreen texture during resolve.
        if (desc.interlockMode == gpu::InterlockMode::rasterOrdering)
        {
            assert(!desc.fixedFunctionColorOutput);
            if (auto framebufferRenderTarget =
                    lite_rtti_cast<FramebufferRenderTargetGL*>(
                        static_cast<RenderTargetGL*>(desc.renderTarget)))
            {
                // We rendered to an offscreen texture. Copy back to the
                // external target framebuffer.
                framebufferRenderTarget->bindTextureFramebuffer(
                    GL_READ_FRAMEBUFFER);
                framebufferRenderTarget->bindDestinationFramebuffer(
                    GL_DRAW_FRAMEBUFFER);
                renderContextImpl->state()->setPipelineState(
                    gpu::COLOR_ONLY_PIPELINE_STATE);
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

private:
    constexpr static uint32_t PLS_TRANSIENT_BACKING_CLEAR_IDX = 0;
    constexpr static uint32_t PLS_ATOMIC_COVERAGE_CLEAR_IDX =
        PLS_TRANSIENT_BACKING_MAX_DEPTH;
    constexpr static uint32_t PLS_CLEAR_BUFFER_COUNT =
        PLS_TRANSIENT_BACKING_MAX_DEPTH + 1;

    glutils::Texture m_plsTransientBackingTexture = glutils::Texture::Zero();
    glutils::Texture m_atomicCoverageTexture = glutils::Texture::Zero();
    glutils::Framebuffer m_plsClearFBO; // FBO solely for clearing PLS.
};

std::unique_ptr<RenderContextGLImpl::PixelLocalStorageImpl>
RenderContextGLImpl::MakePLSImplRWTexture()
{
    return std::make_unique<PLSImplRWTexture>();
}
} // namespace rive::gpu

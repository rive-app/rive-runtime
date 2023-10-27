/*
 * Copyright 2023 Rive
 */

#include "rive/pls/gl/pls_render_context_gl_impl.hpp"

#include "shaders/constants.glsl"

#include "../out/obj/generated/glsl.exports.h"

namespace rive::pls
{
constexpr static GLenum kPLSDrawBuffers[4] = {GL_COLOR_ATTACHMENT0,
                                              GL_COLOR_ATTACHMENT1,
                                              GL_COLOR_ATTACHMENT2,
                                              GL_COLOR_ATTACHMENT3};

class PLSRenderContextGLImpl::PLSImplRWTexture : public PLSRenderContextGLImpl::PLSImpl
{
    rcp<PLSRenderTargetGL> wrapGLRenderTarget(GLuint framebufferID,
                                              size_t width,
                                              size_t height,
                                              const PlatformFeatures&) override
    {
        // For now, the main framebuffer also has to be an RW texture.
        return nullptr;
    }

    rcp<PLSRenderTargetGL> makeOffscreenRenderTarget(
        size_t width,
        size_t height,
        PLSRenderTargetGL::TargetTextureOwnership targetTextureOwnership,
        const PlatformFeatures& platformFeatures) override
    {
        auto renderTarget =
            rcp(new PLSRenderTargetGL(width, height, targetTextureOwnership, platformFeatures));
        glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, width);
        glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, height);
        renderTarget->allocateCoverageBackingTextures();

        renderTarget->createSideFramebuffer();
        glDrawBuffers(4, kPLSDrawBuffers);
        return renderTarget;
    }

    void activatePixelLocalStorage(PLSRenderContextGLImpl*,
                                   const PLSRenderContext::FlushDescriptor& desc) override
    {
        auto renderTarget = static_cast<const PLSRenderTargetGL*>(desc.renderTarget);

        // Clear the necessary textures.
        constexpr static GLuint kZero[4]{};
        glBindFramebuffer(GL_FRAMEBUFFER, renderTarget->sideFramebufferID());
        renderTarget->reattachTargetTextureIfDifferent();
        if (desc.loadAction == LoadAction::clear)
        {
            float clearColor4f[4];
            UnpackColorToRGBA32F(desc.clearColor, clearColor4f);
            glClearBufferfv(GL_COLOR, FRAMEBUFFER_PLANE_IDX, clearColor4f);
        }
        glClearBufferuiv(GL_COLOR, COVERAGE_PLANE_IDX, kZero);
        if (desc.needsClipBuffer)
        {
            glClearBufferuiv(GL_COLOR, CLIP_PLANE_IDX, kZero);
        }

        // Bind the RW textures.
        glBindImageTexture(FRAMEBUFFER_PLANE_IDX,
                           renderTarget->m_targetTextureID,
                           0,
                           GL_FALSE,
                           0,
                           GL_READ_WRITE,
                           GL_RGBA8);
        glBindImageTexture(COVERAGE_PLANE_IDX,
                           renderTarget->m_coverageTextureID,
                           0,
                           GL_FALSE,
                           0,
                           GL_READ_WRITE,
                           GL_R32UI);
        glBindImageTexture(ORIGINAL_DST_COLOR_PLANE_IDX,
                           renderTarget->m_originalDstColorTextureID,
                           0,
                           GL_FALSE,
                           0,
                           GL_READ_WRITE,
                           GL_RGBA8);
        if (desc.needsClipBuffer)
        {
            glBindImageTexture(CLIP_PLANE_IDX,
                               renderTarget->m_clipTextureID,
                               0,
                               GL_FALSE,
                               0,
                               GL_READ_WRITE,
                               GL_R32UI);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, renderTarget->drawFramebufferID());
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    void deactivatePixelLocalStorage(PLSRenderContextGLImpl*) override
    {
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
    }

    const char* shaderDefineName() const override { return GLSL_PLS_IMPL_RW_TEXTURE; }

    void onBarrier() override { return glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT); }
};

std::unique_ptr<PLSRenderContextGLImpl::PLSImpl> PLSRenderContextGLImpl::MakePLSImplRWTexture()
{
    return std::make_unique<PLSImplRWTexture>();
}
} // namespace rive::pls

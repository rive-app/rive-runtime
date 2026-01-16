/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/gl/render_target_gl.hpp"

#include "rive/renderer/gpu.hpp"
#include "rive/renderer/gl/render_context_gl_impl.hpp"
#include "shaders/constants.glsl"

namespace rive::gpu
{
GLuint RenderTargetGL::dstColorTexture()
{
    if (m_dstColorTexture == 0)
    {
        m_dstColorTexture = glutils::Texture();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_dstColorTexture);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width(), height());
    }
    return m_dstColorTexture;
}

void RenderTargetGL::bindDstColorFramebuffer(GLenum target)
{
    if (m_dstColorFramebuffer == 0)
    {
        m_dstColorFramebuffer = glutils::Framebuffer();
        glBindFramebuffer(target, m_dstColorFramebuffer);
        glFramebufferTexture2D(target,
                               GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D,
                               dstColorTexture(),
                               0);
    }
    else
    {
        glBindFramebuffer(target, m_dstColorFramebuffer);
    }
}

TextureRenderTargetGL::~TextureRenderTargetGL() {}

void TextureRenderTargetGL::bindTextureFramebuffer(GLenum target)
{
    if (m_framebufferID == 0)
    {
        m_framebufferID = glutils::Framebuffer();
    }
    glBindFramebuffer(target, m_framebufferID);

    if (m_framebufferTargetAttachmentDirty)
    {
        glFramebufferTexture2D(target,
                               GL_COLOR_ATTACHMENT0 + COLOR_PLANE_IDX,
                               GL_TEXTURE_2D,
                               m_externalTextureID,
                               0);
        m_framebufferTargetAttachmentDirty = false;
    }
}

void TextureRenderTargetGL::bindHeadlessFramebuffer(
    const GLCapabilities& capabilities)
{
    if (m_headlessFramebuffer == 0)
    {
        m_headlessFramebuffer = glutils::Framebuffer();
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_headlessFramebuffer);
#ifndef RIVE_WEBGL
        if (capabilities.ARB_shader_image_load_store)
        {
            glFramebufferParameteri(GL_DRAW_FRAMEBUFFER,
                                    GL_FRAMEBUFFER_DEFAULT_WIDTH,
                                    width());
            glFramebufferParameteri(GL_DRAW_FRAMEBUFFER,
                                    GL_FRAMEBUFFER_DEFAULT_HEIGHT,
                                    height());
        }
#endif
        glDrawBuffers(0, nullptr);
    }
    else
    {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_headlessFramebuffer);
    }

#ifdef GL_ANGLE_shader_pixel_local_storage
    if (capabilities.ANGLE_shader_pixel_local_storage &&
        m_webglPLSBindingsDirty)
    {
        glFramebufferTexturePixelLocalStorageANGLE(COLOR_PLANE_IDX,
                                                   m_externalTextureID,
                                                   0,
                                                   0);
        glFramebufferTexturePixelLocalStorageANGLE(COVERAGE_PLANE_IDX,
                                                   m_webglPLSBackingR32UI,
                                                   0,
                                                   0);
        if (!capabilities.avoidTexture2DArrayWithWebGLPLS)
        {
            glFramebufferTexturePixelLocalStorageANGLE(CLIP_PLANE_IDX,
                                                       m_webglPLSBackingR32UI,
                                                       0,
                                                       1);
        }
        else
        {
            glFramebufferTexturePixelLocalStorageANGLE(
                CLIP_PLANE_IDX,
                m_webglPLSBackingR32UIFallback,
                0,
                0);
        }
        glFramebufferTexturePixelLocalStorageANGLE(SCRATCH_COLOR_PLANE_IDX,
                                                   m_webglPLSBackingRGBA8,
                                                   0,
                                                   0);
        m_webglPLSBindingsDirty = false;
    }
#endif
}

RenderTargetGL::MSAAResolveAction TextureRenderTargetGL::bindMSAAFramebuffer(
    RenderContextGLImpl* renderContextImpl,
    int sampleCount,
    const IAABB* preserveBounds,
    bool* isFBO0)
{
    assert(sampleCount > 0);
    if (m_msaaFramebuffer == 0)
    {
        m_msaaFramebuffer = glutils::Framebuffer();
    }

    if (isFBO0 != nullptr)
    {
        *isFBO0 = false;
    }

    sampleCount = std::max(sampleCount, 1);
    if (m_msaaFramebufferSampleCount != sampleCount)
    {
        m_msaaDepthStencilBuffer = glutils::Renderbuffer();
        glBindRenderbuffer(GL_RENDERBUFFER, m_msaaDepthStencilBuffer);

        glBindFramebuffer(GL_FRAMEBUFFER, m_msaaFramebuffer);
#ifndef RIVE_WEBGL
        if (renderContextImpl->capabilities()
                .EXT_multisampled_render_to_texture)
        {
            glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER,
                                                sampleCount,
                                                GL_DEPTH24_STENCIL8,
                                                width(),
                                                height());

            // With EXT_multisampled_render_to_texture we can render directly to
            // the target texture.
            glFramebufferTexture2DMultisampleEXT(GL_FRAMEBUFFER,
                                                 GL_COLOR_ATTACHMENT0,
                                                 GL_TEXTURE_2D,
                                                 m_externalTextureID,
                                                 0,
                                                 sampleCount);
        }
        else
#endif
        {
            glRenderbufferStorageMultisample(GL_RENDERBUFFER,
                                             sampleCount,
                                             GL_DEPTH24_STENCIL8,
                                             width(),
                                             height());

            // Render to an offscreen renderbuffer that gets resolved into the
            // target texture.
            m_msaaColorBuffer = glutils::Renderbuffer();
            glBindRenderbuffer(GL_RENDERBUFFER, m_msaaColorBuffer);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER,
                                             sampleCount,
                                             GL_RGBA8,
                                             width(),
                                             height());
            glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                      GL_COLOR_ATTACHMENT0,
                                      GL_RENDERBUFFER,
                                      m_msaaColorBuffer);
        }
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                  GL_DEPTH_STENCIL_ATTACHMENT,
                                  GL_RENDERBUFFER,
                                  m_msaaDepthStencilBuffer);

        m_msaaFramebufferSampleCount = sampleCount;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_msaaFramebuffer);

    if (renderContextImpl->capabilities().EXT_multisampled_render_to_texture)
    {
        return MSAAResolveAction::automatic; // MSAA render-to-texture resolves
                                             // automatically.
    }
    else
    {
        if (preserveBounds != nullptr)
        {
            // The MSAA render target is offscreen. In order to preserve, we
            // need to draw the target texture into the MSAA buffer.
            // (glBlitFramebuffer() doesn't support texture -> MSAA.)
            renderContextImpl->blitTextureToFramebufferAsDraw(
                m_externalTextureID,
                *preserveBounds,
                height());
        }

        return MSAAResolveAction::framebufferBlit; // Caller must resolve this
                                                   // framebuffer.
    }
}

#ifdef GL_ANGLE_shader_pixel_local_storage
void TextureRenderTargetGL::allocateWebGLPLSBacking(
    const GLCapabilities& capabilities)
{
    if (m_webglPLSBackingR32UI == 0)
    {
        glActiveTexture(GL_TEXTURE0);
        m_webglPLSBackingR32UI = glutils::Texture();
        if (!capabilities.avoidTexture2DArrayWithWebGLPLS)
        {
            glBindTexture(GL_TEXTURE_2D_ARRAY, m_webglPLSBackingR32UI);
            glTexStorage3D(GL_TEXTURE_2D_ARRAY,
                           1,
                           GL_R32UI,
                           width(),
                           height(),
                           2);
        }
        else
        {
            // ANGLE_shader_pixel_local_storage is currently broken with
            // GL_TEXTURE_2D_ARRAY on ANGLE's d3d11 renderer.
            m_webglPLSBackingR32UIFallback = glutils::Texture();
            glBindTexture(GL_TEXTURE_2D, m_webglPLSBackingR32UI);
            glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, width(), height());
            glBindTexture(GL_TEXTURE_2D, m_webglPLSBackingR32UIFallback);
            glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, width(), height());
        }
        m_webglPLSBindingsDirty = true;
    }
    if (m_webglPLSBackingRGBA8 == 0)
    {
        m_webglPLSBackingRGBA8 = glutils::Texture();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_webglPLSBackingRGBA8);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width(), height());
        m_webglPLSBindingsDirty = true;
    }
}
#endif

FramebufferRenderTargetGL::~FramebufferRenderTargetGL() {}

void FramebufferRenderTargetGL::bindDestinationFramebuffer(GLenum target)
{
    glBindFramebuffer(target, m_externalFramebufferID);
}

void FramebufferRenderTargetGL::allocateOffscreenTargetTexture()
{
    if (m_offscreenTargetTexture == 0)
    {
        m_offscreenTargetTexture = glutils::Texture();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_offscreenTargetTexture);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width(), height());
        m_textureRenderTarget.setTargetTexture(m_offscreenTargetTexture);
    }
}

GLuint FramebufferRenderTargetGL::renderTexture()
{
    allocateOffscreenTargetTexture();
    return m_textureRenderTarget.renderTexture();
}

void FramebufferRenderTargetGL::bindTextureFramebuffer(GLenum target)
{
    allocateOffscreenTargetTexture();
    m_textureRenderTarget.bindTextureFramebuffer(target);
}

void FramebufferRenderTargetGL::bindHeadlessFramebuffer(
    const GLCapabilities& capabilities)
{
    m_textureRenderTarget.bindHeadlessFramebuffer(capabilities);
}

RenderTargetGL::MSAAResolveAction FramebufferRenderTargetGL::
    bindMSAAFramebuffer(RenderContextGLImpl* renderContextImpl,
                        int sampleCount,
                        const IAABB* preserveBounds,
                        bool* isFBO0)
{
    assert(sampleCount > 0);
    if (m_sampleCount > 1)
    {
        // Just bind the destination framebuffer it's already msaa, even if its
        // sampleCount doesn't match the desired count.
        bindDestinationFramebuffer(GL_FRAMEBUFFER);
        if (isFBO0 != nullptr)
        {
            *isFBO0 = m_externalFramebufferID == 0;
        }
        return MSAAResolveAction::automatic;
    }
    else
    {
        // The destination framebuffer is not multisampled. Bind the offscreen
        // one.
        if (preserveBounds != nullptr)
        {
            // API support for copying a non-msaa framebuffer into an msaa
            // framebuffer (for preservation) is awful. It needs to be done in 2
            // steps:
            //   1. Blit non-msaa framebuffer -> texture.
            //   2. Draw texture -> msaa framebuffer.
            // (NOTE: step 2 gets skipped when we have
            // EXT_multisampled_render_to_texture.)
            allocateOffscreenTargetTexture();
            m_textureRenderTarget.bindTextureFramebuffer(GL_DRAW_FRAMEBUFFER);
            bindDestinationFramebuffer(GL_READ_FRAMEBUFFER);
            renderContextImpl->state()->setPipelineState(
                gpu::COLOR_ONLY_PIPELINE_STATE);
            glutils::BlitFramebuffer(
                *preserveBounds,
                height()); // Step 1.
                           // Step 2 will happen when we bind.
        }
        else if (renderContextImpl->capabilities()
                     .EXT_multisampled_render_to_texture)
        {
            // When we have EXT_multisampled_render_to_texture, the "msaa
            // buffer" is just the target texture.
            allocateOffscreenTargetTexture();
        }
        m_textureRenderTarget.bindMSAAFramebuffer(renderContextImpl,
                                                  sampleCount,
                                                  preserveBounds,
                                                  isFBO0);
        // Since we're rendering to an offscreen framebuffer, the client has to
        // resolve this buffer even if we have
        // EXT_multisampled_render_to_texture.
        return MSAAResolveAction::framebufferBlit;
    }
}

#ifdef GL_ANGLE_shader_pixel_local_storage
void FramebufferRenderTargetGL::allocateWebGLPLSBacking(
    const GLCapabilities& capabilities)
{
    m_textureRenderTarget.allocateWebGLPLSBacking(capabilities);
}
#endif
} // namespace rive::gpu

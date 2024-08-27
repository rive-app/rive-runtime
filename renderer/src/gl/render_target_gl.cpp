/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/gl/render_target_gl.hpp"

#include "rive/renderer/gpu.hpp"
#include "rive/renderer/gl/render_context_gl_impl.hpp"
#include "shaders/constants.glsl"

namespace rive::gpu
{
TextureRenderTargetGL::~TextureRenderTargetGL() {}

static glutils::Texture make_backing_texture(GLenum internalformat, uint32_t width, uint32_t height)
{
    glutils::Texture texture;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexStorage2D(GL_TEXTURE_2D, 1, internalformat, width, height);
    return texture;
}

void TextureRenderTargetGL::allocateInternalPLSTextures(gpu::InterlockMode interlockMode)
{
    if (m_coverageTexture == 0)
    {
        m_coverageTexture = make_backing_texture(GL_R32UI, width(), height());
        m_framebufferInternalAttachmentsDirty = true;
        m_framebufferInternalPLSBindingsDirty = true;
    }
    if (m_clipTexture == 0)
    {
        m_clipTexture = make_backing_texture(GL_R32UI, width(), height());
        m_framebufferInternalAttachmentsDirty = true;
        m_framebufferInternalPLSBindingsDirty = true;
    }
    if (interlockMode == InterlockMode::rasterOrdering && m_scratchColorTexture == 0)
    {
        m_scratchColorTexture = make_backing_texture(GL_RGBA8, width(), height());
        m_framebufferInternalAttachmentsDirty = true;
        m_framebufferInternalPLSBindingsDirty = true;
    }
}

void TextureRenderTargetGL::bindInternalFramebuffer(GLenum target, DrawBufferMask drawBufferMask)
{
    if (m_framebufferID == 0)
    {
        m_framebufferID = glutils::Framebuffer();
    }
    glBindFramebuffer(target, m_framebufferID);

    if (target != GL_READ_FRAMEBUFFER && m_internalDrawBufferMask != drawBufferMask)
    {
        GLenum drawBufferList[4];
        for (int i = 0; i < 4; ++i)
        {
            drawBufferList[i] = (drawBufferMask & static_cast<DrawBufferMask>(1 << i))
                                    ? GL_COLOR_ATTACHMENT0 + i
                                    : GL_NONE;
            static_assert((int)DrawBufferMask::color == 1 << COLOR_PLANE_IDX);
            static_assert((int)DrawBufferMask::clip == 1 << CLIP_PLANE_IDX);
            static_assert((int)DrawBufferMask::scratchColor == 1 << SCRATCH_COLOR_PLANE_IDX);
            static_assert((int)DrawBufferMask::coverage == 1 << COVERAGE_PLANE_IDX);
        }
        glDrawBuffers(4, drawBufferList);
        m_internalDrawBufferMask = drawBufferMask;
    }

    if (m_framebufferTargetAttachmentDirty)
    {
        glFramebufferTexture2D(target,
                               GL_COLOR_ATTACHMENT0 + COLOR_PLANE_IDX,
                               GL_TEXTURE_2D,
                               m_externalTextureID,
                               0);
        m_framebufferTargetAttachmentDirty = false;
    }

    if (m_framebufferInternalAttachmentsDirty)
    {
        glFramebufferTexture2D(target,
                               GL_COLOR_ATTACHMENT0 + CLIP_PLANE_IDX,
                               GL_TEXTURE_2D,
                               m_clipTexture,
                               0);
        glFramebufferTexture2D(target,
                               GL_COLOR_ATTACHMENT0 + SCRATCH_COLOR_PLANE_IDX,
                               GL_TEXTURE_2D,
                               m_scratchColorTexture,
                               0);
        glFramebufferTexture2D(target,
                               GL_COLOR_ATTACHMENT0 + COVERAGE_PLANE_IDX,
                               GL_TEXTURE_2D,
                               m_coverageTexture,
                               0);
        m_framebufferInternalAttachmentsDirty = false;
    }
}

void TextureRenderTargetGL::bindHeadlessFramebuffer(const GLCapabilities& capabilities)
{
    if (m_headlessFramebuffer == 0)
    {
        m_headlessFramebuffer = glutils::Framebuffer();
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_headlessFramebuffer);
#ifndef RIVE_WEBGL
        if (capabilities.ARB_shader_image_load_store)
        {
            glFramebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, width());
            glFramebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, height());
        }
#endif
        glDrawBuffers(0, nullptr);
    }
    else
    {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_headlessFramebuffer);
    }

#ifdef GL_ANGLE_shader_pixel_local_storage
    if (capabilities.ANGLE_shader_pixel_local_storage)
    {
        if (m_framebufferTargetPLSBindingDirty)
        {
            glFramebufferTexturePixelLocalStorageANGLE(COLOR_PLANE_IDX, m_externalTextureID, 0, 0);
            m_framebufferTargetPLSBindingDirty = false;
        }

        if (m_framebufferInternalPLSBindingsDirty)
        {
            glFramebufferTexturePixelLocalStorageANGLE(CLIP_PLANE_IDX, m_clipTexture, 0, 0);
            glFramebufferTexturePixelLocalStorageANGLE(SCRATCH_COLOR_PLANE_IDX,
                                                       m_scratchColorTexture,
                                                       0,
                                                       0);
            glFramebufferTexturePixelLocalStorageANGLE(COVERAGE_PLANE_IDX, m_coverageTexture, 0, 0);
            m_framebufferInternalPLSBindingsDirty = false;
        }
    }
#endif
}

void TextureRenderTargetGL::bindAsImageTextures(DrawBufferMask drawBufferMask)
{
#ifndef RIVE_WEBGL
    if (drawBufferMask & DrawBufferMask::color)
    {
        assert(m_externalTextureID != 0);
        glBindImageTexture(COLOR_PLANE_IDX,
                           m_externalTextureID,
                           0,
                           GL_FALSE,
                           0,
                           GL_READ_WRITE,
                           GL_RGBA8);
    }
    if (drawBufferMask & DrawBufferMask::clip)
    {
        assert(m_clipTexture != 0);
        glBindImageTexture(CLIP_PLANE_IDX, m_clipTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
    }
    if (drawBufferMask & DrawBufferMask::scratchColor)
    {
        assert(m_scratchColorTexture != 0);
        glBindImageTexture(SCRATCH_COLOR_PLANE_IDX,
                           m_scratchColorTexture,
                           0,
                           GL_FALSE,
                           0,
                           GL_READ_WRITE,
                           GL_RGBA8);
    }
    if (drawBufferMask & DrawBufferMask::coverage)
    {
        assert(m_coverageTexture != 0);
        glBindImageTexture(COVERAGE_PLANE_IDX,
                           m_coverageTexture,
                           0,
                           GL_FALSE,
                           0,
                           GL_READ_WRITE,
                           GL_R32UI);
    }
#endif
}

PLSRenderTargetGL::MSAAResolveAction TextureRenderTargetGL::bindMSAAFramebuffer(
    PLSRenderContextGLImpl* plsContextGL,
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
        if (plsContextGL->capabilities().EXT_multisampled_render_to_texture)
        {
            glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER,
                                                sampleCount,
                                                GL_DEPTH24_STENCIL8,
                                                width(),
                                                height());

            // With EXT_multisampled_render_to_texture we can render directly to the target texture.
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

            // Render to an offscreen renderbuffer that gets resolved into the target texture.
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

    if (plsContextGL->capabilities().EXT_multisampled_render_to_texture)
    {
        return MSAAResolveAction::automatic; // MSAA render-to-texture resolves automatically.
    }
    else
    {
        if (preserveBounds != nullptr)
        {
            // The MSAA render target is offscreen. In order to preserve, we need to draw the target
            // texture into the MSAA buffer. (glBlitFramebuffer() doesn't support texture -> MSAA.)
            plsContextGL->blitTextureToFramebufferAsDraw(m_externalTextureID,
                                                         *preserveBounds,
                                                         height());
        }

        return MSAAResolveAction::framebufferBlit; // Caller must resolve this framebuffer.
    }
}

void TextureRenderTargetGL::bindInternalDstTexture(GLenum activeTexture)
{
    glActiveTexture(activeTexture);
    glBindTexture(GL_TEXTURE_2D, m_externalTextureID);
}

FramebufferRenderTargetGL::~FramebufferRenderTargetGL() {}

void FramebufferRenderTargetGL::bindDestinationFramebuffer(GLenum target)
{
    glBindFramebuffer(target, m_externalFramebufferID);
}

void FramebufferRenderTargetGL::allocateOffscreenTargetTexture()
{
    if (m_offscreenTargetTexture == 0)
    {
        m_offscreenTargetTexture = make_backing_texture(GL_RGBA8, width(), height());
        m_textureRenderTarget.setTargetTexture(m_offscreenTargetTexture);
    }
}

void FramebufferRenderTargetGL::allocateInternalPLSTextures(gpu::InterlockMode interlockMode)
{
    m_textureRenderTarget.allocateInternalPLSTextures(interlockMode);
}

void FramebufferRenderTargetGL::bindInternalFramebuffer(GLenum target,
                                                        DrawBufferMask drawBufferMask)
{

    m_textureRenderTarget.bindInternalFramebuffer(target, drawBufferMask);
}

void FramebufferRenderTargetGL::bindHeadlessFramebuffer(const GLCapabilities& capabilities)
{
    m_textureRenderTarget.bindHeadlessFramebuffer(capabilities);
}

void FramebufferRenderTargetGL::bindAsImageTextures(DrawBufferMask drawBufferMask)
{
    m_textureRenderTarget.bindAsImageTextures(drawBufferMask);
}

PLSRenderTargetGL::MSAAResolveAction FramebufferRenderTargetGL::bindMSAAFramebuffer(
    PLSRenderContextGLImpl* plsContextGL,
    int sampleCount,
    const IAABB* preserveBounds,
    bool* isFBO0)
{
    assert(sampleCount > 0);
    if (m_sampleCount > 1)
    {
        // Just bind the destination framebuffer it's already msaa, even if its sampleCount doesn't
        // match the desired count.
        bindDestinationFramebuffer(GL_FRAMEBUFFER);
        if (isFBO0 != nullptr)
        {
            *isFBO0 = m_externalFramebufferID == 0;
        }
        return MSAAResolveAction::automatic;
    }
    else
    {
        // The destination framebuffer is not multisampled. Bind the offscreen one.
        if (preserveBounds != nullptr)
        {
            // API support for copying a non-msaa framebuffer into an msaa framebuffer (for
            // preservation) is awful. It needs to be done in 2 steps:
            //   1. Blit non-msaa framebuffer -> texture.
            //   2. Draw texture -> msaa framebuffer.
            // (NOTE: step 2 gets skipped when we have EXT_multisampled_render_to_texture.)
            allocateOffscreenTargetTexture();
            m_textureRenderTarget.bindInternalFramebuffer(GL_DRAW_FRAMEBUFFER,
                                                          DrawBufferMask::color);
            bindDestinationFramebuffer(GL_READ_FRAMEBUFFER);
            glutils::BlitFramebuffer(*preserveBounds, height()); // Step 1.
                                                                 // Step 2 will happen when we bind.
        }
        else if (plsContextGL->capabilities().EXT_multisampled_render_to_texture)
        {
            // When we have EXT_multisampled_render_to_texture, the "msaa buffer" is just the target
            // texture.
            allocateOffscreenTargetTexture();
        }
        m_textureRenderTarget.bindMSAAFramebuffer(plsContextGL,
                                                  sampleCount,
                                                  preserveBounds,
                                                  isFBO0);
        // Since we're rendering to an offscreen framebuffer, the client has to resolve this buffer
        // even if we have EXT_multisampled_render_to_texture.
        return MSAAResolveAction::framebufferBlit;
    }
}

void FramebufferRenderTargetGL::bindInternalDstTexture(GLenum activeTexture)
{
    allocateOffscreenTargetTexture();
    m_textureRenderTarget.bindInternalDstTexture(activeTexture);
}
} // namespace rive::gpu

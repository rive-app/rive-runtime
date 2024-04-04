/*
 * Copyright 2023 Rive
 */

#include "rive/pls/gl/pls_render_target_gl.hpp"

#include "rive/pls/pls.hpp"
#include "rive/pls/gl/pls_render_context_gl_impl.hpp"
#include "shaders/constants.glsl"
#include "rive/pls/gl/gl_utils.hpp"

namespace rive::pls
{
TextureRenderTargetGL::~TextureRenderTargetGL()
{
    if (m_framebufferID != 0)
    {
        glDeleteFramebuffers(1, &m_framebufferID);
    }
    if (m_coverageTextureID != 0)
    {
        glDeleteTextures(1, &m_coverageTextureID);
    }
    if (m_clipTextureID != 0)
    {
        glDeleteTextures(1, &m_clipTextureID);
    }
    if (m_originalDstColorTextureID != 0)
    {
        glDeleteTextures(1, &m_originalDstColorTextureID);
    }
    if (m_headlessFramebufferID != 0)
    {
        glDeleteFramebuffers(1, &m_headlessFramebufferID);
    }
    if (m_msaaFramebufferID != 0)
    {
        glDeleteFramebuffers(1, &m_msaaFramebufferID);
    }
}

static GLuint make_backing_texture(GLenum internalformat, uint32_t width, uint32_t height)
{
    GLuint textureID;
    glGenTextures(1, &textureID);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexStorage2D(GL_TEXTURE_2D, 1, internalformat, width, height);
    return textureID;
}

void TextureRenderTargetGL::allocateInternalPLSTextures(pls::InterlockMode interlockMode)
{
    if (m_coverageTextureID == 0)
    {
        m_coverageTextureID = make_backing_texture(GL_R32UI, width(), height());
        m_framebufferInternalAttachmentsDirty = true;
        m_framebufferInternalPLSBindingsDirty = true;
    }
    if (m_clipTextureID == 0)
    {
        m_clipTextureID = make_backing_texture(GL_R32UI, width(), height());
        m_framebufferInternalAttachmentsDirty = true;
        m_framebufferInternalPLSBindingsDirty = true;
    }
    if (interlockMode == InterlockMode::rasterOrdering && m_originalDstColorTextureID == 0)
    {
        m_originalDstColorTextureID = make_backing_texture(GL_RGBA8, width(), height());
        m_framebufferInternalAttachmentsDirty = true;
        m_framebufferInternalPLSBindingsDirty = true;
    }
}

void TextureRenderTargetGL::bindInternalFramebuffer(GLenum target, uint32_t drawBufferCount)
{
    if (m_framebufferID == 0)
    {
        glGenFramebuffers(1, &m_framebufferID);
    }
    glBindFramebuffer(target, m_framebufferID);

    if (target != GL_READ_FRAMEBUFFER && m_internalDrawBufferCount != drawBufferCount)
    {
        assert(drawBufferCount <= 4);
        drawBufferCount = std::min(drawBufferCount, 4u);
        constexpr static GLenum kDrawBufferList[4] = {GL_COLOR_ATTACHMENT0,
                                                      GL_COLOR_ATTACHMENT1,
                                                      GL_COLOR_ATTACHMENT2,
                                                      GL_COLOR_ATTACHMENT3};
        glDrawBuffers(drawBufferCount, kDrawBufferList);
        m_internalDrawBufferCount = drawBufferCount;
    }

    if (m_framebufferTargetAttachmentDirty)
    {
        glFramebufferTexture2D(target,
                               GL_COLOR_ATTACHMENT0 + FRAMEBUFFER_PLANE_IDX,
                               GL_TEXTURE_2D,
                               m_externalTextureID,
                               0);
        m_framebufferTargetAttachmentDirty = false;
    }

    if (m_framebufferInternalAttachmentsDirty)
    {
        glFramebufferTexture2D(target,
                               GL_COLOR_ATTACHMENT0 + COVERAGE_PLANE_IDX,
                               GL_TEXTURE_2D,
                               m_coverageTextureID,
                               0);
        glFramebufferTexture2D(target,
                               GL_COLOR_ATTACHMENT0 + CLIP_PLANE_IDX,
                               GL_TEXTURE_2D,
                               m_clipTextureID,
                               0);
        glFramebufferTexture2D(target,
                               GL_COLOR_ATTACHMENT0 + ORIGINAL_DST_COLOR_PLANE_IDX,
                               GL_TEXTURE_2D,
                               m_originalDstColorTextureID,
                               0);
        m_framebufferInternalAttachmentsDirty = false;
    }
}

void TextureRenderTargetGL::bindHeadlessFramebuffer(const GLCapabilities& capabilities)
{
    if (m_headlessFramebufferID == 0)
    {
        glGenFramebuffers(1, &m_headlessFramebufferID);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_headlessFramebufferID);
#ifdef RIVE_DESKTOP_GL
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
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_headlessFramebufferID);
    }

#ifdef GL_ANGLE_shader_pixel_local_storage
    if (capabilities.ANGLE_shader_pixel_local_storage)
    {
        if (m_framebufferTargetPLSBindingDirty)
        {
            glFramebufferTexturePixelLocalStorageANGLE(FRAMEBUFFER_PLANE_IDX,
                                                       m_externalTextureID,
                                                       0,
                                                       0);
            m_framebufferTargetPLSBindingDirty = false;
        }

        if (m_framebufferInternalPLSBindingsDirty)
        {
            glFramebufferTexturePixelLocalStorageANGLE(COVERAGE_PLANE_IDX,
                                                       m_coverageTextureID,
                                                       0,
                                                       0);
            glFramebufferTexturePixelLocalStorageANGLE(CLIP_PLANE_IDX, m_clipTextureID, 0, 0);
            glFramebufferTexturePixelLocalStorageANGLE(ORIGINAL_DST_COLOR_PLANE_IDX,
                                                       m_originalDstColorTextureID,
                                                       0,
                                                       0);
            m_framebufferInternalPLSBindingsDirty = false;
        }
    }
#endif
}

void TextureRenderTargetGL::bindAsImageTextures()
{
#ifdef RIVE_DESKTOP_GL
    if (m_externalTextureID != 0)
    {
        glBindImageTexture(FRAMEBUFFER_PLANE_IDX,
                           m_externalTextureID,
                           0,
                           GL_FALSE,
                           0,
                           GL_READ_WRITE,
                           GL_RGBA8);
    }
    if (m_coverageTextureID != 0)
    {
        glBindImageTexture(COVERAGE_PLANE_IDX,
                           m_coverageTextureID,
                           0,
                           GL_FALSE,
                           0,
                           GL_READ_WRITE,
                           GL_R32UI);
    }
    if (m_clipTextureID != 0)
    {
        glBindImageTexture(CLIP_PLANE_IDX,
                           m_clipTextureID,
                           0,
                           GL_FALSE,
                           0,
                           GL_READ_WRITE,
                           GL_R32UI);
    }
    if (m_originalDstColorTextureID != 0)
    {
        glBindImageTexture(ORIGINAL_DST_COLOR_PLANE_IDX,
                           m_originalDstColorTextureID,
                           0,
                           GL_FALSE,
                           0,
                           GL_READ_WRITE,
                           GL_RGBA8);
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
    if (m_msaaFramebufferID == 0)
    {
        glGenFramebuffers(1, &m_msaaFramebufferID);
    }

    if (isFBO0 != nullptr)
    {
        *isFBO0 = false;
    }

    sampleCount = std::max(sampleCount, 1);
    if (m_msaaFramebufferSampleCount != sampleCount)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_msaaFramebufferID);

        GLuint msaaColorBuffer = 0;
        if (plsContextGL->capabilities().EXT_multisampled_render_to_texture)
        {
            // With EXT_multisampled_render_to_texture we can render directly to the target texture.
            glFramebufferTexture2DMultisampleEXT(GL_FRAMEBUFFER,
                                                 GL_COLOR_ATTACHMENT0,
                                                 GL_TEXTURE_2D,
                                                 m_externalTextureID,
                                                 0,
                                                 sampleCount);
        }
        else
        {
            // Render to an offscreen renderbuffer that gets resolved into the target texture.
            glGenRenderbuffers(1, &msaaColorBuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, msaaColorBuffer);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER,
                                             sampleCount,
                                             GL_RGBA8,
                                             width(),
                                             height());
            glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                      GL_COLOR_ATTACHMENT0,
                                      GL_RENDERBUFFER,
                                      msaaColorBuffer);
        }

        GLuint msaaDepthStencilBuffer;
        glGenRenderbuffers(1, &msaaDepthStencilBuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, msaaDepthStencilBuffer);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER,
                                         sampleCount,
                                         GL_DEPTH24_STENCIL8,
                                         width(),
                                         height());
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                  GL_DEPTH_STENCIL_ATTACHMENT,
                                  GL_RENDERBUFFER,
                                  msaaDepthStencilBuffer);

        // Bind FBO 0 so we can orphan the MSAA render buffers to our framebuffer.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (msaaColorBuffer != 0)
        {
            glDeleteRenderbuffers(1, &msaaColorBuffer);
        }
        glDeleteRenderbuffers(1, &msaaDepthStencilBuffer);

        m_msaaFramebufferSampleCount = sampleCount;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_msaaFramebufferID);

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

FramebufferRenderTargetGL::~FramebufferRenderTargetGL()
{
    if (m_offscreenTargetTextureID != 0)
    {
        glDeleteTextures(1, &m_offscreenTargetTextureID);
    }
}

void FramebufferRenderTargetGL::bindDestinationFramebuffer(GLenum target)
{
    glBindFramebuffer(target, m_externalFramebufferID);
}

void FramebufferRenderTargetGL::allocateOffscreenTargetTexture()
{
    if (m_offscreenTargetTextureID == 0)
    {
        m_offscreenTargetTextureID = make_backing_texture(GL_RGBA8, width(), height());
        m_textureRenderTarget.setTargetTexture(m_offscreenTargetTextureID);
    }
}

void FramebufferRenderTargetGL::allocateInternalPLSTextures(pls::InterlockMode interlockMode)
{
    m_textureRenderTarget.allocateInternalPLSTextures(interlockMode);
}

void FramebufferRenderTargetGL::bindInternalFramebuffer(GLenum target, uint32_t drawBufferCount)
{

    m_textureRenderTarget.bindInternalFramebuffer(target, drawBufferCount);
}

void FramebufferRenderTargetGL::bindHeadlessFramebuffer(const GLCapabilities& capabilities)
{
    m_textureRenderTarget.bindHeadlessFramebuffer(capabilities);
}

void FramebufferRenderTargetGL::bindAsImageTextures()
{
    m_textureRenderTarget.bindAsImageTextures();
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
            m_textureRenderTarget.bindInternalFramebuffer(GL_DRAW_FRAMEBUFFER, 1);
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
} // namespace rive::pls

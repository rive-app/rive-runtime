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

    if (m_externalTextureAttachmentDirty)
    {
        glFramebufferTexture2D(target,
                               GL_COLOR_ATTACHMENT0 + COLOR_PLANE_IDX,
                               GL_TEXTURE_2D,
                               m_externalTextureID,
                               0);
        m_externalTextureAttachmentDirty = false;
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
                                                   0,
                                                   GL_NONE);
        glFramebufferTexturePixelLocalStorageANGLE(COVERAGE_PLANE_IDX,
                                                   m_webglPLSBackingR32UI,
                                                   0,
                                                   0,
                                                   GL_NONE);
        if (!capabilities.avoidTexture2DArrayWithWebGLPLS)
        {
            glFramebufferTexturePixelLocalStorageANGLE(CLIP_PLANE_IDX,
                                                       m_webglPLSBackingR32UI,
                                                       0,
                                                       1,
                                                       GL_NONE);
        }
        else
        {
            glFramebufferTexturePixelLocalStorageANGLE(
                CLIP_PLANE_IDX,
                m_webglPLSBackingR32UIFallback,
                0,
                0,
                GL_NONE);
        }
        glFramebufferTexturePixelLocalStorageANGLE(SCRATCH_COLOR_PLANE_IDX,
                                                   m_webglPLSBackingRGBA8,
                                                   0,
                                                   0,
                                                   GL_NONE);
        m_webglPLSBindingsDirty = false;
    }
#endif
}

RenderTargetGL::MSAAResolveAction TextureRenderTargetGL::
    bindFramebufferForDepthStencilMode(RenderContextGLImpl* renderContextImpl,
                                       int sampleCount,
                                       const IAABB* preserveBounds,
                                       bool* isFBO0)
{
    assert(sampleCount > 0);
    if (m_dsFBO == 0)
    {
        m_dsFBO = glutils::Framebuffer();
    }

    if (isFBO0 != nullptr)
    {
        *isFBO0 = false;
    }

    sampleCount = std::max(sampleCount, 1);

    glBindFramebuffer(GL_FRAMEBUFFER, m_dsFBO);

    // Update the m_externalTextureID attachment. (Only relevant if we aren't
    // rendering offscreen.)
    if (m_dsFBOExternalTextureAttachmentDirty ||
        m_dsFBOSampleCount != sampleCount)
    {
        if (sampleCount == 1)
        {
            // When sampleCount == 1 we can render directly to the target
            // texture.
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                                   GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D,
                                   m_externalTextureID,
                                   0);
        }
#ifndef RIVE_WEBGL
        else if (renderContextImpl->capabilities()
                     .EXT_multisampled_render_to_texture)
        {
            // With EXT_multisampled_render_to_texture we can render directly to
            // the target texture regardless of sampleCount.
            glFramebufferTexture2DMultisampleEXT(GL_FRAMEBUFFER,
                                                 GL_COLOR_ATTACHMENT0,
                                                 GL_TEXTURE_2D,
                                                 m_externalTextureID,
                                                 0,
                                                 sampleCount);
        }
#endif
        // else we are rendering offscreen, and the caller is responsible to
        // blit our MSAA result into their target texture on their own.

        m_dsFBOExternalTextureAttachmentDirty = false;
    }

    // Update the depthStencil attachment, (and msaa color if we're rendering
    // offscreen).
    if (m_dsFBOSampleCount != sampleCount)
    {
        m_dsFBOColorBuffer = glutils::Renderbuffer::Zero();
        m_dsFBODepthStencilBuffer = glutils::Renderbuffer();

        glBindRenderbuffer(GL_RENDERBUFFER, m_dsFBODepthStencilBuffer);
        if (sampleCount == 1)
        {
            glRenderbufferStorage(GL_RENDERBUFFER,
                                  GL_DEPTH24_STENCIL8,
                                  width(),
                                  height());
        }
#ifndef RIVE_WEBGL
        else if (renderContextImpl->capabilities()
                     .EXT_multisampled_render_to_texture)
        {
            glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER,
                                                sampleCount,
                                                GL_DEPTH24_STENCIL8,
                                                width(),
                                                height());
        }
#endif
        else
        {
            glRenderbufferStorageMultisample(GL_RENDERBUFFER,
                                             sampleCount,
                                             GL_DEPTH24_STENCIL8,
                                             width(),
                                             height());

            // Render to an offscreen renderbuffer that gets resolved into the
            // target texture.
            m_dsFBOColorBuffer = glutils::Renderbuffer();
            glBindRenderbuffer(GL_RENDERBUFFER, m_dsFBOColorBuffer);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER,
                                             sampleCount,
                                             GL_RGBA8,
                                             width(),
                                             height());
            glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                      GL_COLOR_ATTACHMENT0,
                                      GL_RENDERBUFFER,
                                      m_dsFBOColorBuffer);
        }
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                  GL_DEPTH_STENCIL_ATTACHMENT,
                                  GL_RENDERBUFFER,
                                  m_dsFBODepthStencilBuffer);

        m_dsFBOSampleCount = sampleCount;
    }

    if (sampleCount == 1 ||
        renderContextImpl->capabilities().EXT_multisampled_render_to_texture)
    {
        // The caller will draw directly into the destination framebuffer
        // itself.
        return MSAAResolveAction::none;
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
                height(),
                bottomUp());
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

// Returns the bit size of the given framebuffer attachment, or 0 if there isn't
// one.
static GLint attachmentBitSize(GLenum attachment, GLenum sizeParam)
{
    GLint objectType = GL_NONE;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
                                          attachment,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                          &objectType);
    GLint size = 0;
    if (objectType != GL_NONE)
    {
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
                                              attachment,
                                              sizeParam,
                                              &size);
    }
    return size;
}

void FramebufferRenderTargetGL::validateDepthStencilPrecisionOnce()
{
    if (m_didValidateDepthStencilPrecision)
    {
        return;
    }
    m_didValidateDepthStencilPrecision = true;

    // FBO0 names these GL_DEPTH and GL_STENCIL, whereas a user framebuffer
    // object names them GL_DEPTH_ATTACHMENT and GL_STENCIL_ATTACHMENT.
    const bool isFBO0 = m_externalFramebufferID == 0;
    const GLint depthSize =
        attachmentBitSize(isFBO0 ? GL_DEPTH : GL_DEPTH_ATTACHMENT,
                          GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE);
    const GLint stencilSize =
        attachmentBitSize(isFBO0 ? GL_STENCIL : GL_STENCIL_ATTACHMENT,
                          GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE);

    if (depthSize < 24)
    {
        fprintf(stderr,
                "RIVE WARNING: Rive requires at least 24 bits of depth "
                "precision (%d provided).\n",
                depthSize);
        fflush(stderr);
    }

    if (stencilSize < 8)
    {
        fprintf(stderr,
                "RIVE WARNING: Rive requires at least 8 bits of stencil "
                "precision (%d provided).\n",
                stencilSize);
        fflush(stderr);
    }
}

RenderTargetGL::MSAAResolveAction FramebufferRenderTargetGL::
    bindFramebufferForDepthStencilMode(RenderContextGLImpl* renderContextImpl,
                                       int desiredSampleCount,
                                       const IAABB* preserveBounds,
                                       bool* isFBO0)
{
    assert(desiredSampleCount > 0);
    if (desiredSampleCount == 1 || // Non-MSAA always renders directly to the
                                   // destination framebuffer (even if the
                                   // framebuffer is MSAA).
        m_sampleCount > 1) // Always render to the destination framebuffer if
                           // it's already MSAA (even if its sampleCount doesn't
                           // match the desired count).
    {
        bindDestinationFramebuffer(GL_FRAMEBUFFER);
        // The renderTarget's depth/stencil belong to the client -- warn if they
        // don't have enough bits.
        validateDepthStencilPrecisionOnce();
        if (isFBO0 != nullptr)
        {
            *isFBO0 = m_externalFramebufferID == 0;
        }
        return MSAAResolveAction::none;
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
            glutils::BlitFramebuffer(*preserveBounds,
                                     height(),
                                     bottomUp()); // Step 1.
            // Step 2 will happen when we bind.
        }
        else if (renderContextImpl->capabilities()
                     .EXT_multisampled_render_to_texture)
        {
            // When we have EXT_multisampled_render_to_texture, the "msaa
            // buffer" is just the target texture.
            allocateOffscreenTargetTexture();
        }
        m_textureRenderTarget.bindFramebufferForDepthStencilMode(
            renderContextImpl,
            desiredSampleCount,
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

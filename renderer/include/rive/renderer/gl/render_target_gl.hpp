/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/renderer/gpu.hpp"
#include "rive/renderer/render_target.hpp"
#include "rive/renderer/gl/gles3.hpp"
#include "rive/renderer/gl/gl_utils.hpp"
#include "utils/lite_rtti.hpp"

namespace rive::gpu
{
class PLSRenderContextGLImpl;

class PLSRenderTargetGL : public PLSRenderTarget, public enable_lite_rtti<PLSRenderTargetGL>
{
public:
    // Bind a framebuffer whose color attachment is the final rendering destination.
    // When rendering to an external framebuffer, this is that same framebuffer.
    // When rendering to a texture, it's an internal framebuffer that targets the texture.
    virtual void bindDestinationFramebuffer(GLenum target) = 0;

    // Ensures backing textures for the internal PLS planes are allocated.
    // Does not allocate an offscreen target texture.
    // Does not allocate an "scratchColor" texture if InterlockMode is experimentalAtomics.
    virtual void allocateInternalPLSTextures(gpu::InterlockMode) = 0;

    // Specifies which PLS planes to enable when a render target is bound.
    enum class DrawBufferMask
    {
        color = 1 << 0,
        clip = 1 << 1,
        scratchColor = 1 << 2,
        coverage = 1 << 3,

        rasterOrderingBuffers = color | coverage | clip | scratchColor,
        atomicBuffers = color | coverage | clip,
    };

    // Binds a framebuffer with all allocated textures attached as color attachments. Used for
    // clearing, blitting, and for rendering with EXT_shader_framebuffer_fetch.
    //
    // 'drawBufferCount' specifies the number of glDrawBuffers to enable, starting with
    // GL_COLOR_ATTACHMENT0. Ignored for GL_READ_FRAMEBUFFER.
    virtual void bindInternalFramebuffer(GLenum target, DrawBufferMask) = 0;

    // Binds a "headless" framebuffer to GL_DRAW_FRAMEBUFFER with no color attachments and no
    // enabled draw buffers.
    //
    // If ARB_shader_image_load_store is supported, GL_FRAMEBUFFER_DEFAULT_WIDTH/HEIGHT will be the
    // render target's dimensions.
    //
    // If ANGLE_shader_pixel_local_storage is supported, the allocated textures will also be bound
    // to their corresponding pixel local storage planes.
    virtual void bindHeadlessFramebuffer(const GLCapabilities&) = 0;

    // Binds the allocated textures to their corresponding GL image binding slots.
    virtual void bindAsImageTextures(DrawBufferMask) = 0;

    enum class MSAAResolveAction
    {
        automatic,       // The MSAA framebuffer will be resolved automatically.
        framebufferBlit, // Caller must call glBlitFramebuffer() to resolve the MSAA framebuffer.
    };

    // Bind the renderTarget as a multisampled framebuffer.
    //
    // If the msaa framebuffer is offscreen, returns MSAAResolveAction::framebufferBlit, indicating
    // that the caller must resolve it when done.
    virtual MSAAResolveAction bindMSAAFramebuffer(PLSRenderContextGLImpl*,
                                                  int sampleCount,
                                                  const IAABB* preserveBounds = nullptr,
                                                  bool* isFBO0 = nullptr) = 0;

    // Binds the internal framebuffer as a texture that can be used to fetch the destination color
    // (for blending).
    virtual void bindInternalDstTexture(GLenum activeTexture) = 0;

protected:
    PLSRenderTargetGL(uint32_t width, uint32_t height) : PLSRenderTarget(width, height) {}
};

RIVE_MAKE_ENUM_BITSET(PLSRenderTargetGL::DrawBufferMask);

// GL render target that draws to an external texture provided by the client.
//
// Client must call setTargetTexture() before using this render target.
class TextureRenderTargetGL : public lite_rtti_override<PLSRenderTargetGL, TextureRenderTargetGL>
{
public:
    TextureRenderTargetGL(uint32_t width, uint32_t height) : lite_rtti_override(width, height) {}
    ~TextureRenderTargetGL();

    GLuint externalTextureID() const { return m_externalTextureID; }

    // Set the texture being rendered to. Ownership of this object is not assumed; the caller must
    // delete it when done.
    void setTargetTexture(GLuint externalTextureID)
    {
        m_externalTextureID = externalTextureID;
        m_framebufferTargetAttachmentDirty = true;
        m_framebufferTargetPLSBindingDirty = true;
    }

    void bindDestinationFramebuffer(GLenum target) final
    {
        bindInternalFramebuffer(target, DrawBufferMask::color);
    }
    void allocateInternalPLSTextures(gpu::InterlockMode) final;
    void bindInternalFramebuffer(GLenum target, DrawBufferMask) final;
    void bindHeadlessFramebuffer(const GLCapabilities&) final;
    void bindAsImageTextures(DrawBufferMask) final;
    MSAAResolveAction bindMSAAFramebuffer(PLSRenderContextGLImpl*,
                                          int sampleCount,
                                          const IAABB* preserveBounds,
                                          bool* isFBO0) final;
    void bindInternalDstTexture(GLenum activeTexture) final;

private:
    // Not owned or deleted by us.
    GLuint m_externalTextureID = 0;

    glutils::Framebuffer m_framebufferID = glutils::Framebuffer::Zero();
    glutils::Texture m_coverageTexture = glutils::Texture::Zero();
    glutils::Texture m_clipTexture = glutils::Texture::Zero();
    glutils::Texture m_scratchColorTexture = glutils::Texture::Zero();
    glutils::Framebuffer m_headlessFramebuffer = glutils::Framebuffer::Zero();

    // GL enables the first drawBuffer by default.
    DrawBufferMask m_internalDrawBufferMask = DrawBufferMask::color;

    // For framebuffer color attachments.
    bool m_framebufferTargetAttachmentDirty = false;
    bool m_framebufferInternalAttachmentsDirty = false;

    // For ANGLE_shader_pixel_local_storage attachments.
    bool m_framebufferTargetPLSBindingDirty = false;
    bool m_framebufferInternalPLSBindingsDirty = false;

    glutils::Framebuffer m_msaaFramebuffer = glutils::Framebuffer::Zero();
    glutils::Renderbuffer m_msaaColorBuffer = glutils::Renderbuffer::Zero();
    glutils::Renderbuffer m_msaaDepthStencilBuffer = glutils::Renderbuffer::Zero();
    int m_msaaFramebufferSampleCount = 0;
};

// GL render target that draws to an external, immutable FBO provided by the client. Since the
// external FBO is immutable and usually not readable either, it is almost always better to use
// TextureRenderTargetGL if possible.
class FramebufferRenderTargetGL
    : public lite_rtti_override<PLSRenderTargetGL, FramebufferRenderTargetGL>
{
public:
    FramebufferRenderTargetGL(uint32_t width,
                              uint32_t height,
                              GLuint externalFramebufferID,
                              uint32_t sampleCount) :
        lite_rtti_override(width, height),
        m_externalFramebufferID(externalFramebufferID),
        m_sampleCount(sampleCount),
        m_textureRenderTarget(width, height)
    {}

    ~FramebufferRenderTargetGL();

    uint32_t sampleCount() const { return m_sampleCount; }

    // Ensures a texture is allocated that mirrors our external FBO. (We can't modify the external
    // FBO and usually can't read it either, so we often have no choice but to render to an
    // offscreen texture first.)
    void allocateOffscreenTargetTexture();

    void bindDestinationFramebuffer(GLenum target) final;
    void allocateInternalPLSTextures(gpu::InterlockMode) final;
    void bindInternalFramebuffer(GLenum target, DrawBufferMask) final;
    void bindHeadlessFramebuffer(const GLCapabilities&) final;
    void bindAsImageTextures(DrawBufferMask) final;
    MSAAResolveAction bindMSAAFramebuffer(PLSRenderContextGLImpl*,
                                          int sampleCount,
                                          const IAABB* preserveBounds,
                                          bool* isFBO0) final;
    void bindInternalDstTexture(GLenum activeTexture) final;

private:
    // Ownership of this object is not assumed; the client must delete it when done.
    const GLuint m_externalFramebufferID;
    const uint32_t m_sampleCount;

    // Holds the PLS textures we might need.
    TextureRenderTargetGL m_textureRenderTarget;

    // Created for m_textureRenderTarget if/when we can't render directly to the framebuffer.
    glutils::Texture m_offscreenTargetTexture = glutils::Texture::Zero();
};
} // namespace rive::gpu

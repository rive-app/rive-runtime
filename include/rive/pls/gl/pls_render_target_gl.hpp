/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/pls/pls.hpp"
#include "rive/pls/pls_render_target.hpp"
#include "rive/pls/gl/gles3.hpp"
#include "utils/lite_rtti.hpp"

namespace rive::pls
{
class PLSRenderTargetGL : public PLSRenderTarget, public enable_lite_rtti<PLSRenderTargetGL>
{
public:
    // Bind a framebuffer whose color attachment is the final rendering destination.
    // When rendering to an external framebuffer, this is that same framebuffer.
    // When rendering to a texture, it's an internal framebuffer that targets the texture.
    virtual void bindDestinationFramebuffer(GLenum target) = 0;

    // Ensures backing textures for the internal PLS planes are allocated.
    // Does not allocate an offscreen target texture.
    // Does not allocate an "originalDstColor" texture if InterlockMode is experimentalAtomics.
    virtual void allocateInternalPLSTextures(pls::InterlockMode) = 0;

    // Binds a framebuffer with all allocated textures attached as color attachments. Used for
    // clearing, blitting, and for rendering with EXT_shader_framebuffer_fetch.
    //
    // 'drawBufferCount' specifies the number of glDrawBuffers to enable, starting with
    // GL_COLOR_ATTACHMENT0. Ignored for GL_READ_FRAMEBUFFER.
    virtual void bindInternalFramebuffer(GLenum target, uint32_t drawBufferCount) = 0;

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
    virtual void bindAsImageTextures() = 0;

    // Bind the renderTarget as a multisampled framebuffer.
    // When rendering to an external framebuffer, this is that same framebuffer (assumed MSAA).
    // When rendering to a texture, it's an offscreen MSAA renderbuffer that the caller must resolve
    // into the target texture.
    virtual void bindMSAAFramebuffer(GLenum target, int sampleCount, const GLCapabilities&) = 0;

protected:
    PLSRenderTargetGL(uint32_t width, uint32_t height) : PLSRenderTarget(width, height) {}
};

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

    void bindDestinationFramebuffer(GLenum target) final { bindInternalFramebuffer(target, 1); }
    void allocateInternalPLSTextures(pls::InterlockMode) final;
    void bindInternalFramebuffer(GLenum target, uint32_t drawBufferCount) final;
    void bindHeadlessFramebuffer(const GLCapabilities&) final;
    void bindAsImageTextures() final;
    void bindMSAAFramebuffer(GLenum target, int sampleCount, const GLCapabilities&) final;

private:
    // Not owned or deleted by us.
    GLuint m_externalTextureID = 0;

    GLuint m_framebufferID = 0;
    GLuint m_coverageTextureID = 0;
    GLuint m_clipTextureID = 0;
    GLuint m_originalDstColorTextureID = 0;
    GLuint m_headlessFramebufferID = 0;

    uint32_t m_internalDrawBufferCount = 1; // GL enables 1 draw buffer by default.

    // For framebuffer color attachments.
    bool m_framebufferTargetAttachmentDirty = false;
    bool m_framebufferInternalAttachmentsDirty = false;

    // For ANGLE_shader_pixel_local_storage attachments.
    bool m_framebufferTargetPLSBindingDirty = false;
    bool m_framebufferInternalPLSBindingsDirty = false;

    GLuint m_msaaFramebufferID = 0;
    int m_msaaFramebufferSampleCount = 0;
};

// GL render target that draws to an external, immutable FBO provided by the client. Since the
// external FBO is immutable and usually not readable either, it is almost always better to use
// TextureRenderTargetGL if possible.
class FramebufferRenderTargetGL
    : public lite_rtti_override<PLSRenderTargetGL, FramebufferRenderTargetGL>
{
public:
    FramebufferRenderTargetGL(uint32_t width, uint32_t height, GLuint externalFramebufferID) :
        lite_rtti_override(width, height),
        m_externalFramebufferID(externalFramebufferID),
        m_textureRenderTarget(width, height)
    {}

    ~FramebufferRenderTargetGL();

    // Ensures a texture is allocated that mirrors our external FBO. (We can't modify the external
    // FBO and usually can't read it either, so we often have no choice but to render to an
    // offscreen texture first.)
    void allocateOffscreenTargetTexture();

    void bindDestinationFramebuffer(GLenum target) final;
    void allocateInternalPLSTextures(pls::InterlockMode) final;
    void bindInternalFramebuffer(GLenum target, uint32_t drawBufferCount) final;
    void bindHeadlessFramebuffer(const GLCapabilities&) final;
    void bindAsImageTextures() final;
    void bindMSAAFramebuffer(GLenum target, int sampleCount, const GLCapabilities&) final
    {
        bindDestinationFramebuffer(target); // Assume the external framebuffer is MSAA.
    }

private:
    // Ownership of this object is not assumed; the client must delete it when done.
    const GLuint m_externalFramebufferID;

    // Holds the PLS textures we might need.
    TextureRenderTargetGL m_textureRenderTarget;

    // Created for m_textureRenderTarget if/when we can't render directly to the framebuffer.
    GLuint m_offscreenTargetTextureID = 0;
};
} // namespace rive::pls

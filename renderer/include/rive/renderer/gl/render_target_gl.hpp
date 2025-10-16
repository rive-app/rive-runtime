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
class RenderContextGLImpl;

class RenderTargetGL : public RenderTarget,
                       public ENABLE_LITE_RTTI(RenderTargetGL)
{
public:
    // Bind a framebuffer whose color attachment is the final rendering
    // destination. When rendering to an external framebuffer, this is that same
    // framebuffer. When rendering to a texture, it's an internal framebuffer
    // that targets the texture.
    virtual void bindDestinationFramebuffer(GLenum target) = 0;

    // Returns a texture handle that can be rendered to (e.g., for rendering via
    // shader images, ANGLE_shader_pixel_local_storage, or
    // EXT_multisampled_render_to_texture). When rendering to an external
    // framebuffer, this is an internal texture allocation that the caller must
    // blit back to the main framebuffer at the end of a render pass. Otherwise,
    // this is the same target texture provided by the client.
    virtual GLuint renderTexture() = 0;

    // Binds a framebuffer with renderTexture() attached. When rendering to an
    // external framebuffer, this is an internal FBO that the caller must blit
    // back to the main framebuffer at the end of a render pass. Otherwise, it's
    // the main framebuffer we render to normally.
    virtual void bindTextureFramebuffer(GLenum target) = 0;

    // Binds a "headless" framebuffer to GL_DRAW_FRAMEBUFFER with no color
    // attachments and no enabled draw buffers.
    //
    // If ARB_shader_image_load_store is supported,
    // GL_FRAMEBUFFER_DEFAULT_WIDTH/HEIGHT will be the render target's
    // dimensions.
    //
    // If ANGLE_shader_pixel_local_storage is supported, the allocated textures
    // will also be bound to their corresponding pixel local storage planes.
    virtual void bindHeadlessFramebuffer(const GLCapabilities&) = 0;

    enum class MSAAResolveAction
    {
        automatic,       // The MSAA framebuffer will be resolved automatically.
        framebufferBlit, // Caller must call glBlitFramebuffer() to resolve the
                         // MSAA framebuffer.
    };

    // Bind the renderTarget as a multisampled framebuffer.
    //
    // If the msaa framebuffer is offscreen, returns
    // MSAAResolveAction::framebufferBlit, indicating that the caller must
    // resolve it when done.
    virtual MSAAResolveAction bindMSAAFramebuffer(
        RenderContextGLImpl*,
        int sampleCount,
        const IAABB* preserveBounds = nullptr,
        bool* isFBO0 = nullptr) = 0;

#ifdef GL_ANGLE_shader_pixel_local_storage
    // ANGLE_shader_pixel_local_storage requires all backing textures in a
    // render pass to have identical dimensions, so we can't use the context's
    // "TransientPLSBacking" resource. Instead, this method ensures backing
    // textures of identical width & height to the render target are allocated.
    virtual void allocateWebGLPLSBacking(const GLCapabilities&) = 0;
#endif

protected:
    RenderTargetGL(uint32_t width, uint32_t height) :
        RenderTarget(width, height)
    {}
};

// GL render target that draws to an external texture provided by the client.
//
// Client must call setTargetTexture() before using this render target.
class TextureRenderTargetGL
    : public LITE_RTTI_OVERRIDE(RenderTargetGL, TextureRenderTargetGL)
{
public:
    TextureRenderTargetGL(uint32_t width, uint32_t height) :
        lite_rtti_override(width, height)
    {}
    ~TextureRenderTargetGL();

    GLuint externalTextureID() const { return m_externalTextureID; }

    // Set the texture being rendered to. Ownership of this object is not
    // assumed; the caller must delete it when done.
    void setTargetTexture(GLuint externalTextureID)
    {
        m_externalTextureID = externalTextureID;
        m_framebufferTargetAttachmentDirty = true;
        m_webglPLSBindingsDirty = true;
    }

    void bindDestinationFramebuffer(GLenum target) final
    {
        bindTextureFramebuffer(target);
    }
    GLuint renderTexture() final { return externalTextureID(); }
    void bindTextureFramebuffer(GLenum target) final;
    void bindHeadlessFramebuffer(const GLCapabilities&) final;
    MSAAResolveAction bindMSAAFramebuffer(RenderContextGLImpl*,
                                          int sampleCount,
                                          const IAABB* preserveBounds,
                                          bool* isFBO0) final;

#ifdef GL_ANGLE_shader_pixel_local_storage
    void allocateWebGLPLSBacking(const GLCapabilities&) final;
#endif

private:
    // Not owned or deleted by us.
    GLuint m_externalTextureID = 0;

    glutils::Framebuffer m_framebufferID = glutils::Framebuffer::Zero();
    glutils::Framebuffer m_headlessFramebuffer = glutils::Framebuffer::Zero();

    // For framebuffer color attachments.
    bool m_framebufferTargetAttachmentDirty = false;

    // For ANGLE_shader_pixel_local_storage attachments.
    glutils::Texture m_webglPLSBackingR32UI = glutils::Texture::Zero();
    // Fallback for when backing PLS planes with GL_TEXTURE_2D_ARRAY is broken
    // (i.e., on ANGLE's d3d11 renderer).
    glutils::Texture m_webglPLSBackingR32UIFallback = glutils::Texture::Zero();
    glutils::Texture m_webglPLSBackingRGBA8 = glutils::Texture::Zero();
    bool m_webglPLSBindingsDirty = false;

    glutils::Framebuffer m_msaaFramebuffer = glutils::Framebuffer::Zero();
    glutils::Renderbuffer m_msaaColorBuffer = glutils::Renderbuffer::Zero();
    glutils::Renderbuffer m_msaaDepthStencilBuffer =
        glutils::Renderbuffer::Zero();
    int m_msaaFramebufferSampleCount = 0;
};

// GL render target that draws to an external, immutable FBO provided by the
// client. Since the external FBO is immutable and usually not readable either,
// it is almost always better to use TextureRenderTargetGL if possible.
class FramebufferRenderTargetGL
    : public LITE_RTTI_OVERRIDE(RenderTargetGL, FramebufferRenderTargetGL)
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

    // Ensures a texture is allocated that mirrors our external FBO. (We can't
    // modify the external FBO and usually can't read it either, so we often
    // have no choice but to render to an offscreen texture first.)
    void allocateOffscreenTargetTexture();

    void bindDestinationFramebuffer(GLenum target) final;
    GLuint renderTexture() final;
    void bindTextureFramebuffer(GLenum target) final;
    void bindHeadlessFramebuffer(const GLCapabilities&) final;
    MSAAResolveAction bindMSAAFramebuffer(RenderContextGLImpl*,
                                          int sampleCount,
                                          const IAABB* preserveBounds,
                                          bool* isFBO0) final;

#ifdef GL_ANGLE_shader_pixel_local_storage
    void allocateWebGLPLSBacking(const GLCapabilities&) final;
#endif

private:
    // Ownership of this object is not assumed; the client must delete it when
    // done.
    const GLuint m_externalFramebufferID;
    const uint32_t m_sampleCount;

    // Holds the PLS textures we might need.
    TextureRenderTargetGL m_textureRenderTarget;

    // Created for m_textureRenderTarget if/when we can't render directly to the
    // framebuffer.
    glutils::Texture m_offscreenTargetTexture = glutils::Texture::Zero();
};
} // namespace rive::gpu

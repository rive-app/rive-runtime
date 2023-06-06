/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/pls/pls.hpp"
#include "rive/pls/pls_render_target.hpp"
#include "rive/pls/gl/gles3.hpp"

namespace rive::pls
{
class PLSRenderTargetGL : public PLSRenderTarget
{
public:
    ~PLSRenderTargetGL() override;

    // GL framebuffer ID to use when drawing to this render target via pixel local storage.
    GLuint drawFramebufferID() const { return m_drawFramebufferID; }

    // GL framebuffer ID to use when reading from this render target via glBlitFramebuffer().
    GLuint readFramebufferID() const { return m_readFramebufferID; }

private:
    friend class PLSRenderContextGL;

    // Creates a render target that draws to an existing GL framebuffer. The caller must also call
    // allocateCoverageBackingTextures() and attach those textures to the framebuffer if needed.
    PLSRenderTargetGL(GLuint framebufferID, size_t width, size_t height, const PlatformFeatures&);

    // Creates a render target that draws to a new, offscreen GL framebuffer. The caller must also
    // call allocateCoverageBackingTextures() and attach those textures to the framebuffer if
    // needed, as well as calling createReadFramebuffer() if needed.
    PLSRenderTargetGL(size_t width, size_t height, const PlatformFeatures&);

    // Called when the GL context can't support memoryless pixel local storage. Creates and attaches
    // backing textures for our transient PLS planes that calculate coverage.
    void allocateCoverageBackingTextures();

    // Called when m_offscreenTextureID is not on the main framebuffer as a normal color attachment.
    // Creates a separate framebuffer for reading, with only m_offscreenTextureID attached to
    // GL_COLOR_ATTACHMENT0.
    void createReadFramebuffer();

    GLuint allocateBackingTexture(GLenum internalformat);

    GLuint m_drawFramebufferID = 0;

    // Separate framebuffer with m_offscreenTextureID attached to GL_COLOR_ATTACHMENT0. We need this
    // separate framebuffer for ANGLE_shader_pixel_local_storage because it doesn't load/store from
    // standard GL framebuffers.
    GLuint m_readFramebufferID = 0;

    GLuint m_offscreenTextureID = 0; // Zero if the framebuffer is wrapped.
    GLuint m_coverageTextureID = 0;
    GLuint m_originalDstColorTextureID = 0;
    GLuint m_clipTextureID = 0;

    // Determines whether we should delete m_drawFramebufferID in the destructor.
    bool m_ownsDrawFramebuffer;
};
} // namespace rive::pls

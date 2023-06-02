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
    GLuint readFramebufferID() const
    {
#ifdef RIVE_WEBGL
        return m_readFramebufferID;
#else
        return m_drawFramebufferID;
#endif
    }

private:
    friend class PLSRenderContextGL;
    friend class PLSRenderContextESNative;

    // Can our transient pixel local storage planes be memoryless?
    enum class CoverageBackingType : bool
    {
        texture,
        memoryless
    };

    // Create a render target that draws to an existing GL framebuffer.
    PLSRenderTargetGL(GLuint framebufferID,
                      size_t width,
                      size_t height,
                      CoverageBackingType,
                      const PlatformFeatures&);

    // Create a render target that draws to a new, offscreen GL framebuffer.
    PLSRenderTargetGL(size_t width, size_t height, CoverageBackingType, const PlatformFeatures&);

    // Called when the GL context can't support memoryless pixel local storage. Creates and attaches
    // backing textures for our transient PLS planes.
    void allocateBackingTextures();
    GLuint allocateBackingTexture(GLsizei bindingIdx, GLenum internalformat);
#ifdef RIVE_WEBGL
    void createReadFramebuffer();
#endif

    GLuint m_drawFramebufferID = 0;
#ifdef RIVE_WEBGL
    // Separate framebuffer with m_offscreenTextureID attached to GL_COLOR_ATTACHMENT0. We need this
    // separate framebuffer for ANGLE_shader_pixel_local_storage because it doesn't load/store from
    // standard GL framebuffers.
    GLuint m_readFramebufferID = 0;
#endif
    GLuint m_offscreenTextureID = 0;
    GLuint m_coverageTextureID = 0;
    GLuint m_originalDstColorTextureID = 0;
    GLuint m_clipTextureID = 0;

    // Determines whether we should delete m_drawFramebufferID in the destructor.
    bool m_ownsDrawFramebuffer;
};
} // namespace rive::pls

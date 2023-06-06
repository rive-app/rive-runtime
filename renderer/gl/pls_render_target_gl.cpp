/*
 * Copyright 2023 Rive
 */

#include "rive/pls/gl/pls_render_target_gl.hpp"

#include "rive/pls/pls.hpp"

namespace rive::pls
{
PLSRenderTargetGL::PLSRenderTargetGL(GLuint framebufferID,
                                     size_t width,
                                     size_t height,
                                     const PlatformFeatures& platformFeatures) :
    PLSRenderTarget(width, height),
    m_drawFramebufferID(framebufferID),
    m_readFramebufferID(framebufferID),
    m_ownsDrawFramebuffer(false)
{}

PLSRenderTargetGL::PLSRenderTargetGL(size_t width,
                                     size_t height,
                                     const PlatformFeatures& platformFeatures) :
    PLSRenderTarget(width, height), m_ownsDrawFramebuffer(true)
{
    glGenFramebuffers(1, &m_drawFramebufferID);
    glBindFramebuffer(GL_FRAMEBUFFER, m_drawFramebufferID);
    m_offscreenTextureID = allocateBackingTexture(GL_RGBA8);
    m_readFramebufferID = m_drawFramebufferID;
}

PLSRenderTargetGL::~PLSRenderTargetGL()
{
    if (m_ownsDrawFramebuffer)
    {
        glDeleteFramebuffers(1, &m_drawFramebufferID);
    }
    if (m_readFramebufferID != m_drawFramebufferID)
    {
        glDeleteFramebuffers(1, &m_readFramebufferID);
    }
    glDeleteTextures(1, &m_offscreenTextureID);
    glDeleteTextures(1, &m_coverageTextureID);
    glDeleteTextures(1, &m_originalDstColorTextureID);
    glDeleteTextures(1, &m_clipTextureID);
}

void PLSRenderTargetGL::allocateCoverageBackingTextures()
{
    m_coverageTextureID = allocateBackingTexture(GL_R32UI);
    m_originalDstColorTextureID = allocateBackingTexture(GL_RGBA8);
    m_clipTextureID = allocateBackingTexture(GL_R32UI);
}

GLuint PLSRenderTargetGL::allocateBackingTexture(GLenum internalformat)
{
    GLuint textureID;
    glGenTextures(1, &textureID);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexStorage2D(GL_TEXTURE_2D, 1, internalformat, width(), height());
    return textureID;
}

void PLSRenderTargetGL::createReadFramebuffer()
{
    assert(m_offscreenTextureID);
    glGenFramebuffers(1, &m_readFramebufferID);
    glBindFramebuffer(GL_FRAMEBUFFER, m_readFramebufferID);
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           m_offscreenTextureID,
                           0);
}
} // namespace rive::pls

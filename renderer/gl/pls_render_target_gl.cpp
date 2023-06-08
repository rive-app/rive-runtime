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
    m_sideFramebufferID(framebufferID),
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
    m_sideFramebufferID = m_drawFramebufferID;
}

PLSRenderTargetGL::~PLSRenderTargetGL()
{
    if (m_ownsDrawFramebuffer)
    {
        glDeleteFramebuffers(1, &m_drawFramebufferID);
    }
    if (m_sideFramebufferID != m_drawFramebufferID)
    {
        glDeleteFramebuffers(1, &m_sideFramebufferID);
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

void PLSRenderTargetGL::createSideFramebuffer()
{
    assert(m_offscreenTextureID);
    glGenFramebuffers(1, &m_sideFramebufferID);
    glBindFramebuffer(GL_FRAMEBUFFER, m_sideFramebufferID);
    attachTexturesToCurrentFramebuffer();
}

void PLSRenderTargetGL::attachTexturesToCurrentFramebuffer()
{

    if (m_offscreenTextureID != 0)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0 + kFramebufferPlaneIdx,
                               GL_TEXTURE_2D,
                               m_offscreenTextureID,
                               0);
    }
    if (m_coverageTextureID != 0)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0 + kCoveragePlaneIdx,
                               GL_TEXTURE_2D,
                               m_coverageTextureID,
                               0);
    }
    if (m_originalDstColorTextureID != 0)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0 + kOriginalDstColorPlaneIdx,
                               GL_TEXTURE_2D,
                               m_originalDstColorTextureID,
                               0);
    }
    if (m_clipTextureID != 0)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0 + kClipPlaneIdx,
                               GL_TEXTURE_2D,
                               m_clipTextureID,
                               0);
    }
}
} // namespace rive::pls

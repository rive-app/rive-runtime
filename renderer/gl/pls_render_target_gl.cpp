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
                                     CoverageBackingType coverageBackingType,
                                     const PlatformFeatures& platformFeatures) :
    PLSRenderTarget(width, height), m_drawFramebufferID(framebufferID), m_ownsDrawFramebuffer(false)
{
    if (coverageBackingType == CoverageBackingType::texture)
    {
        allocateBackingTextures();
    }
#ifdef RIVE_WEBGL
    createReadFramebuffer();
#endif
}

PLSRenderTargetGL::PLSRenderTargetGL(size_t width,
                                     size_t height,
                                     CoverageBackingType coverageBackingType,
                                     const PlatformFeatures& platformFeatures) :
    PLSRenderTarget(width, height), m_ownsDrawFramebuffer(true)
{
    glGenFramebuffers(1, &m_drawFramebufferID);
    glBindFramebuffer(GL_FRAMEBUFFER, m_drawFramebufferID);
    m_offscreenTextureID = allocateBackingTexture(kFramebufferPlaneIdx, GL_RGBA8);
    if (coverageBackingType == CoverageBackingType::texture)
    {
        allocateBackingTextures();
    }
#ifdef RIVE_WEBGL
    createReadFramebuffer();
#endif
}

PLSRenderTargetGL::~PLSRenderTargetGL()
{
    if (m_ownsDrawFramebuffer)
    {
        glDeleteFramebuffers(1, &m_drawFramebufferID);
    }
#ifdef RIVE_WEBGL
    glDeleteFramebuffers(1, &m_readFramebufferID);
#endif
    glDeleteTextures(1, &m_coverageTextureID);
    glDeleteTextures(1, &m_originalDstColorTextureID);
    glDeleteTextures(1, &m_clipTextureID);
}

void PLSRenderTargetGL::allocateBackingTextures()
{
    m_coverageTextureID = allocateBackingTexture(kCoveragePlaneIdx, GL_R32UI);
    m_originalDstColorTextureID = allocateBackingTexture(kOriginalDstColorPlaneIdx, GL_RGBA8);
    m_clipTextureID = allocateBackingTexture(kClipPlaneIdx, GL_R32UI);
}

GLuint PLSRenderTargetGL::allocateBackingTexture(GLsizei bindingIdx, GLenum internalformat)
{
    GLuint textureID;
    glGenTextures(1, &textureID);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexStorage2D(GL_TEXTURE_2D, 1, internalformat, width(), height());
#ifdef RIVE_WEBGL
    glFramebufferTexturePixelLocalStorageWEBGL(bindingIdx, textureID, 0, 0);
#else
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0 + bindingIdx,
                           GL_TEXTURE_2D,
                           textureID,
                           0);
#endif
    return textureID;
}

#ifdef RIVE_WEBGL
void PLSRenderTargetGL::createReadFramebuffer()
{
    glGenFramebuffers(1, &m_readFramebufferID);
    glBindFramebuffer(GL_FRAMEBUFFER, m_readFramebufferID);
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           m_offscreenTextureID,
                           0);
}
#endif
} // namespace rive::pls

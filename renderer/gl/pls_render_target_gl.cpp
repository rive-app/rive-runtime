/*
 * Copyright 2023 Rive
 */

#include "rive/pls/gl/pls_render_target_gl.hpp"

#include "rive/pls/pls.hpp"
#include "shaders/constants.glsl"

namespace rive::pls
{
PLSRenderTargetGL::PLSRenderTargetGL(GLuint framebufferID,
                                     size_t width,
                                     size_t height,
                                     const PlatformFeatures& platformFeatures) :
    PLSRenderTarget(width, height),
    m_ownsDrawFramebuffer(false),
    m_ownsTargetTexture(false),
    m_drawFramebufferID(framebufferID),
    m_sideFramebufferID(framebufferID)
{}

PLSRenderTargetGL::PLSRenderTargetGL(size_t width,
                                     size_t height,
                                     TargetTextureOwnership targetTextureOwnership,
                                     const PlatformFeatures& platformFeatures) :
    PLSRenderTarget(width, height),
    m_ownsDrawFramebuffer(true),
    m_ownsTargetTexture(targetTextureOwnership == TargetTextureOwnership::internal)
{
    glGenFramebuffers(1, &m_drawFramebufferID);
    glBindFramebuffer(GL_FRAMEBUFFER, m_drawFramebufferID);
    m_sideFramebufferID = m_drawFramebufferID;
    if (targetTextureOwnership == TargetTextureOwnership::internal)
    {
        m_targetTextureID = allocateBackingTexture(GL_RGBA8);
        m_targetTextureHasChanged = true;
    }
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
    if (m_ownsTargetTexture)
    {
        glDeleteTextures(1, &m_targetTextureID);
    }
    glDeleteTextures(1, &m_coverageTextureID);
    glDeleteTextures(1, &m_originalDstColorTextureID);
    glDeleteTextures(1, &m_clipTextureID);
}

void PLSRenderTargetGL::setTargetTexture(GLuint id)
{
    assert(m_ownsDrawFramebuffer); // We can't change the target on a wrapped framebuffer.
    assert(!m_ownsTargetTexture);  // Create w/ TargetTextureOwnership::external to use this method.
    m_targetTextureID = id;
    m_targetTextureHasChanged = true;
}

GLuint PLSRenderTargetGL::targetTextureIDIfDifferent() const
{
    if (m_targetTextureHasChanged)
    {
        m_targetTextureHasChanged = false;
        return m_targetTextureID;
    }
    return 0;
}

void PLSRenderTargetGL::reattachTargetTextureIfDifferent() const
{
    if (GLuint newTargetTextureID = targetTextureIDIfDifferent())
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0 + FRAMEBUFFER_PLANE_IDX,
                               GL_TEXTURE_2D,
                               newTargetTextureID,
                               0);
    }
}

void PLSRenderTargetGL::allocateCoverageBackingTextures()
{
    m_coverageTextureID = allocateBackingTexture(GL_R32UI);
    m_originalDstColorTextureID = allocateBackingTexture(GL_RGBA8);
    m_clipTextureID = allocateBackingTexture(GL_R32UI);
}

GLuint PLSRenderTargetGL::allocateBackingTexture(GLenum internalformat) const
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
    glGenFramebuffers(1, &m_sideFramebufferID);
    glBindFramebuffer(GL_FRAMEBUFFER, m_sideFramebufferID);
    attachCoverageTexturesToCurrentFramebuffer();
}

void PLSRenderTargetGL::attachCoverageTexturesToCurrentFramebuffer()
{

    if (m_coverageTextureID != 0)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0 + COVERAGE_PLANE_IDX,
                               GL_TEXTURE_2D,
                               m_coverageTextureID,
                               0);
    }
    if (m_originalDstColorTextureID != 0)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0 + ORIGINAL_DST_COLOR_PLANE_IDX,
                               GL_TEXTURE_2D,
                               m_originalDstColorTextureID,
                               0);
    }
    if (m_clipTextureID != 0)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0 + CLIP_PLANE_IDX,
                               GL_TEXTURE_2D,
                               m_clipTextureID,
                               0);
    }
}
} // namespace rive::pls

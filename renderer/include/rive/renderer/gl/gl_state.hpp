/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/renderer/gl/gles3.hpp"
#include "rive/refcnt.hpp"
#include "rive/shapes/paint/blend_mode.hpp"

namespace rive::gpu
{
// Lightweight wrapper around common GL state.
class GLState : public RefCnt<GLState>
{
public:
    GLState(const GLCapabilities& capabilities) : m_capabilities(capabilities) { invalidate(); }

    void invalidate();

    void setBlendEquation(BlendMode);
    void disableBlending();

    void setWriteMasks(bool colorWriteMask, bool depthWriteMask, GLuint stencilWriteMask);
    void setCullFace(GLenum);

    void bindProgram(GLuint);
    void bindVAO(GLuint);
    void bindBuffer(GLenum target, GLuint);

    void deleteProgram(GLuint);
    void deleteVAO(GLuint);
    void deleteBuffer(GLuint);

private:
    const GLCapabilities m_capabilities;
    GLenum m_blendEquation;
    bool m_colorWriteMask;
    bool m_depthWriteMask;
    GLuint m_stencilWriteMask;
    GLenum m_cullFace;
    GLuint m_boundProgramID;
    GLuint m_boundVAO;
    GLuint m_boundArrayBufferID;
    GLuint m_boundUniformBufferID;
    GLuint m_boundPixelUnpackBufferID;

    struct
    {
        bool blendEquation : 1;
        bool writeMasks : 1;
        bool cullFace : 1;
        bool boundProgramID : 1;
        bool boundVAO : 1;
        bool boundArrayBufferID : 1;
        bool boundUniformBufferID : 1;
        bool boundPixelUnpackBufferID : 1;
    } m_validState;
};
} // namespace rive::gpu

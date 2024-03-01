/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/pls/gl/gles3.hpp"
#include "rive/refcnt.hpp"
#include "rive/shapes/paint/blend_mode.hpp"

namespace rive::pls
{
// Lightweight wrapper around common GL state.
class GLState : public RefCnt<GLState>
{
public:
    GLState(const GLCapabilities& extensions) { invalidate(extensions); }

    void invalidate(const GLCapabilities&);

    void setCullFace(GLenum);

    void setBlendEquation(BlendMode);
    void disableBlending();

    void bindProgram(GLuint);
    void bindVAO(GLuint);
    void bindBuffer(GLenum target, GLuint);

    void deleteProgram(GLuint);
    void deleteVAO(GLuint);
    void deleteBuffer(GLuint);

private:
    GLenum m_cullFace;
    GLenum m_blendEquation;
    GLuint m_boundProgramID;
    GLuint m_boundVAO;
    GLuint m_boundArrayBufferID;
    GLuint m_boundUniformBufferID;
    GLuint m_boundPixelUnpackBufferID;

    struct
    {
        bool cullFace : 1;
        bool blendEquation : 1;
        bool boundProgramID : 1;
        bool boundVAO : 1;
        bool boundArrayBufferID : 1;
        bool boundUniformBufferID : 1;
        bool boundPixelUnpackBufferID : 1;
    } m_validState;
};
} // namespace rive::pls

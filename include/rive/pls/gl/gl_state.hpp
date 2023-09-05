/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/pls/gl/gles3.hpp"
#include "rive/refcnt.hpp"

namespace rive::pls
{
// Lightweight wrapper around common GL state.
class GLState : public RefCnt<GLState>
{
public:
    void enableFaceCulling(bool);

    void deleteProgram(GLuint);
    void deleteVAO(GLuint);
    void deleteBuffer(GLuint);

    void bindProgram(GLuint);
    void bindVAO(GLuint);
    void bindBuffer(GLenum target, GLuint);

private:
    bool m_faceCullingEnabled = false;
    GLuint m_boundProgramID = 0;
    GLuint m_boundVAO = 0;
    GLuint m_boundArrayBuffer = 0;
    GLuint m_boundUniformBuffer = 0;
    GLuint m_boundPixelUnpackBuffer = 0;
};
} // namespace rive::pls

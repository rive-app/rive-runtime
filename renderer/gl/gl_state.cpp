/*
 * Copyright 2023 Rive
 */

#include "rive/pls/gl/gl_state.hpp"

namespace rive::pls
{
void GLState::enableFaceCulling(bool enabled)
{
    if (enabled != m_faceCullingEnabled)
    {
        if (enabled)
            glEnable(GL_CULL_FACE);
        else
            glDisable(GL_CULL_FACE);
        m_faceCullingEnabled = enabled;
    }
}

void GLState::deleteProgram(GLuint programID)
{
    glDeleteProgram(programID);
    if (m_boundProgramID == programID)
        m_boundProgramID = 0;
}

void GLState::deleteVAO(GLuint vao)
{
    glDeleteVertexArrays(1, &vao);
    if (m_boundVAO == vao)
        m_boundVAO = 0;
}

void GLState::deleteBuffer(GLuint bufferID)
{
    glDeleteBuffers(1, &bufferID);
    if (m_boundArrayBuffer == bufferID)
        m_boundArrayBuffer = 0;
    if (m_boundUniformBuffer == bufferID)
        m_boundUniformBuffer = 0;
    if (m_boundPixelUnpackBuffer == bufferID)
        m_boundPixelUnpackBuffer = 0;
}

void GLState::bindProgram(GLuint programID)
{
    if (programID != m_boundProgramID)
    {
        glUseProgram(programID);
        m_boundProgramID = programID;
    }
}

void GLState::bindVAO(GLuint vao)
{
    if (vao != m_boundVAO)
    {
        glBindVertexArray(vao);
        m_boundVAO = vao;
    }
}

void GLState::bindBuffer(GLenum target, GLuint bufferID)
{
    GLuint* cachedBufferBinding;
    switch (target)
    {
        default:
            // Don't track GL_ELEMENT_ARRAY_BUFFER, since it is tied to the VAO state.
            glBindBuffer(target, bufferID);
            return;
        case GL_ARRAY_BUFFER:
            cachedBufferBinding = &m_boundArrayBuffer;
            break;
        case GL_UNIFORM_BUFFER:
            cachedBufferBinding = &m_boundUniformBuffer;
            break;
        case GL_PIXEL_UNPACK_BUFFER:
            cachedBufferBinding = &m_boundPixelUnpackBuffer;
            break;
    }
    if (bufferID != *cachedBufferBinding)
    {
        glBindBuffer(target, bufferID);
        *cachedBufferBinding = bufferID;
    }
}
} // namespace rive::pls

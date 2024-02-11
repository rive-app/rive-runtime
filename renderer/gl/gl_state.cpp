/*
 * Copyright 2023 Rive
 */

#include "rive/pls/gl/gl_state.hpp"

#ifdef RIVE_WEBGL
#include <emscripten.h>

EM_JS(void, set_provoking_vertex_webgl, (GLenum convention), {
    const ext = Module["ctx"].getExtension("WEBGL_provoking_vertex");
    if (ext)
    {
        ext.provokingVertexWEBGL(convention);
    }
});
#endif

namespace rive::pls
{
void GLState::invalidate(const GLCapabilities& extensions)
{
    // Invalidate all cached state.
    memset(&m_validState, 0, sizeof(m_validState));

    // PLS only ever culls the CCW face when culling is enabled.
    glFrontFace(GL_CW);
    glCullFace(GL_BACK);
    glDepthMask(GL_FALSE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    // ANGLE_shader_pixel_local_storage doesn't allow dither.
    glDisable(GL_DITHER);

    // D3D and Metal both have a provoking vertex convention of "first" for flat varyings, and it's
    // very costly for ANGLE to implement the OpenGL convention of "last" on these backends. To
    // workaround this, ANGLE provides the ANGLE_provoking_vertex extension. When this extension is
    // present, we can just set the provoking vertex to "first" and trust that it will be fast.
#ifdef RIVE_WEBGL
    set_provoking_vertex_webgl(GL_FIRST_VERTEX_CONVENTION_WEBGL);
#elif defined(RIVE_DESKTOP_GL)
    if (extensions.ANGLE_provoking_vertex)
    {
        glProvokingVertexANGLE(GL_FIRST_VERTEX_CONVENTION_ANGLE);
    }
#endif

    // Low-effort attempt to reset core state we don't use to default values.
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    glDisable(GL_RASTERIZER_DISCARD);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDisable(GL_SAMPLE_COVERAGE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    // glDisable(GL_COLOR_LOGIC_OP);
    // glDisable(GL_INDEX_LOGIC_OP);
    // glDisable(GL_ALPHA_TEST);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
}

void GLState::enableFaceCulling(bool enabled)
{
    if (!m_validState.faceCullingEnabled || enabled != m_faceCullingEnabled)
    {
        if (enabled)
            glEnable(GL_CULL_FACE);
        else
            glDisable(GL_CULL_FACE);
        m_faceCullingEnabled = enabled;
        m_validState.faceCullingEnabled = true;
    }
}

void GLState::bindProgram(GLuint programID)
{
    if (!m_validState.boundProgramID || programID != m_boundProgramID)
    {
        glUseProgram(programID);
        m_boundProgramID = programID;
        m_validState.boundProgramID = true;
    }
}

void GLState::bindVAO(GLuint vao)
{
    if (!m_validState.boundVAO || vao != m_boundVAO)
    {
        glBindVertexArray(vao);
        m_boundVAO = vao;
        m_validState.boundVAO = true;
    }
}

void GLState::bindBuffer(GLenum target, GLuint bufferID)
{
    switch (target)
    {
        default:
            // Don't track GL_ELEMENT_ARRAY_BUFFER, since it is tied to the VAO state.
            glBindBuffer(target, bufferID);
            return;
        case GL_ARRAY_BUFFER:
            if (!m_validState.boundArrayBufferID || bufferID != m_boundArrayBufferID)
            {
                glBindBuffer(GL_ARRAY_BUFFER, bufferID);
                m_boundArrayBufferID = bufferID;
                m_validState.boundArrayBufferID = true;
            }
            break;
        case GL_UNIFORM_BUFFER:
            if (!m_validState.boundUniformBufferID || bufferID != m_boundUniformBufferID)
            {
                glBindBuffer(GL_UNIFORM_BUFFER, bufferID);
                m_boundUniformBufferID = bufferID;
                m_validState.boundUniformBufferID = true;
            }
            break;
        case GL_PIXEL_UNPACK_BUFFER:
            if (!m_validState.boundPixelUnpackBufferID || bufferID != m_boundPixelUnpackBufferID)
            {
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, bufferID);
                m_boundPixelUnpackBufferID = bufferID;
                m_validState.boundPixelUnpackBufferID = true;
            }
            break;
    }
}

void GLState::deleteProgram(GLuint programID)
{
    glDeleteProgram(programID);
    if (m_validState.boundProgramID && m_boundProgramID == programID)
        m_boundProgramID = 0;
}

void GLState::deleteVAO(GLuint vao)
{
    glDeleteVertexArrays(1, &vao);
    if (m_validState.boundVAO && m_boundVAO == vao)
        m_boundVAO = 0;
}

void GLState::deleteBuffer(GLuint bufferID)
{
    glDeleteBuffers(1, &bufferID);
    if (m_validState.boundArrayBufferID && m_boundArrayBufferID == bufferID)
        m_boundArrayBufferID = 0;
    if (m_validState.boundUniformBufferID && m_boundUniformBufferID == bufferID)
        m_boundUniformBufferID = 0;
    if (m_validState.boundPixelUnpackBufferID && m_boundPixelUnpackBufferID == bufferID)
        m_boundPixelUnpackBufferID = 0;
}
} // namespace rive::pls

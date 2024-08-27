/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/gl/gl_state.hpp"

#include "shaders/constants.glsl"

namespace rive::gpu
{
void GLState::invalidate()
{
    // Invalidate all cached state.
    memset(&m_validState, 0, sizeof(m_validState));

    // PLS only ever culls the CCW face when culling is enabled.
    glFrontFace(GL_CW);
    glDepthRangef(0, 1);
    glDepthFunc(GL_LESS);
    glClearDepthf(1);
    glClearStencil(0);

    // We always blend with premultiplied src-over when glBlendFunc is relevant.
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // ANGLE_shader_pixel_local_storage doesn't allow dither.
    glDisable(GL_DITHER);

#ifndef RIVE_ANDROID
    // D3D and Metal both have a provoking vertex convention of "first" for flat varyings, and it's
    // very costly for ANGLE to implement the OpenGL convention of "last" on these backends. To
    // workaround this, ANGLE provides the ANGLE_provoking_vertex extension. When this extension is
    // present, we can just set the provoking vertex to "first" and trust that it will be fast.
    if (m_capabilities.ANGLE_provoking_vertex)
    {
        glProvokingVertexANGLE(GL_FIRST_VERTEX_CONVENTION_ANGLE);
    }
#endif

    // Low-effort attempt to reset core state we don't use to default values.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_POLYGON_OFFSET_FILL);
#ifndef RIVE_WEBGL
    // https://www.khronos.org/registry/webgl/specs/latest/2.0/#5.18
    // WebGL 2.0 behaves as though PRIMITIVE_RESTART_FIXED_INDEX were always enabled.
    glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
#endif
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

constexpr static GLenum blend_mode_to_gl_equation(BlendMode blendMode)
{
    switch (blendMode)
    {
        case BlendMode::srcOver:
            return GL_FUNC_ADD;
        case BlendMode::screen:
            return GL_SCREEN_KHR;
        case BlendMode::overlay:
            return GL_OVERLAY_KHR;
        case BlendMode::darken:
            return GL_DARKEN_KHR;
        case BlendMode::lighten:
            return GL_LIGHTEN_KHR;
        case BlendMode::colorDodge:
            return GL_COLORDODGE_KHR;
        case BlendMode::colorBurn:
            return GL_COLORBURN_KHR;
        case BlendMode::hardLight:
            return GL_HARDLIGHT_KHR;
        case BlendMode::softLight:
            return GL_SOFTLIGHT_KHR;
        case BlendMode::difference:
            return GL_DIFFERENCE_KHR;
        case BlendMode::exclusion:
            return GL_EXCLUSION_KHR;
        case BlendMode::multiply:
            return GL_MULTIPLY_KHR;
        case BlendMode::hue:
            return GL_HSL_HUE_KHR;
        case BlendMode::saturation:
            return GL_HSL_SATURATION_KHR;
        case BlendMode::color:
            return GL_HSL_COLOR_KHR;
        case BlendMode::luminosity:
            return GL_HSL_LUMINOSITY_KHR;
    }
    RIVE_UNREACHABLE();
}

void GLState::setBlendEquation(BlendMode blendMode)
{
    GLenum blendEquation = blend_mode_to_gl_equation(blendMode);
    if (!m_validState.blendEquation || blendEquation != m_blendEquation)
    {
        if (!m_validState.blendEquation || m_blendEquation == GL_NONE)
        {
            glEnable(GL_BLEND);
        }
        glBlendEquation(blendEquation);
        m_blendEquation = blendEquation;
        m_validState.blendEquation = true;
    }
}

void GLState::disableBlending()
{
    if (!m_validState.blendEquation || m_blendEquation != GL_NONE)
    {
        glDisable(GL_BLEND);
        m_blendEquation = GL_NONE;
        m_validState.blendEquation = true;
    }
}

void GLState::setWriteMasks(bool colorWriteMask, bool depthWriteMask, GLuint stencilWriteMask)
{
    if (!m_validState.writeMasks)
    {
        glColorMask(colorWriteMask, colorWriteMask, colorWriteMask, colorWriteMask);
        glDepthMask(depthWriteMask);
        glStencilMask(stencilWriteMask);
        m_colorWriteMask = colorWriteMask;
        m_depthWriteMask = depthWriteMask;
        m_stencilWriteMask = stencilWriteMask;
        m_validState.writeMasks = true;
    }
    else
    {
        if (colorWriteMask != m_colorWriteMask)
        {
            glColorMask(colorWriteMask, colorWriteMask, colorWriteMask, colorWriteMask);
            m_colorWriteMask = colorWriteMask;
        }
        if (depthWriteMask != m_depthWriteMask)
        {
            glDepthMask(depthWriteMask);
            m_depthWriteMask = depthWriteMask;
        }
        if (stencilWriteMask != m_stencilWriteMask)
        {
            glStencilMask(stencilWriteMask);
            m_stencilWriteMask = stencilWriteMask;
        }
    }
}

void GLState::setCullFace(GLenum cullFace)
{
    if (!m_validState.cullFace || cullFace != m_cullFace)
    {
        if (cullFace == GL_NONE)
        {
            glDisable(GL_CULL_FACE);
        }
        else
        {
            if (!m_validState.cullFace || m_cullFace == GL_NONE)
            {
                glEnable(GL_CULL_FACE);
            }
            glCullFace(cullFace);
        }
        m_cullFace = cullFace;
        m_validState.cullFace = true;
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
} // namespace rive::gpu

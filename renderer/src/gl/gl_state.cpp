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

    // ANGLE_shader_pixel_local_storage doesn't allow dither.
    glDisable(GL_DITHER);

    // Low-effort attempt to reset core state we don't use to default values.
    glDisable(GL_POLYGON_OFFSET_FILL);
#ifndef RIVE_WEBGL
    // https://www.khronos.org/registry/webgl/specs/latest/2.0/#5.18
    // WebGL 2.0 behaves as though PRIMITIVE_RESTART_FIXED_INDEX were always
    // enabled.
    glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
#endif
    glDisable(GL_RASTERIZER_DISCARD);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDisable(GL_SAMPLE_COVERAGE);
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
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

#ifndef RIVE_ANDROID
    // D3D and Metal both have a provoking vertex convention of "first" for flat
    // varyings, and it's very costly for ANGLE to implement the OpenGL
    // convention of "last" on these backends. To workaround this, ANGLE
    // provides the ANGLE_provoking_vertex extension. When this extension is
    // present, we can just set the provoking vertex to "first" and trust that
    // it will be fast.
    if (m_capabilities.ANGLE_provoking_vertex)
    {
        glProvokingVertexANGLE(GL_FIRST_VERTEX_CONVENTION_ANGLE);
    }
#endif

    // WebGL doesn't support glMaxShaderCompilerThreadsKHR.
#ifndef RIVE_WEBGL
    if (m_capabilities.KHR_parallel_shader_compile)
    {
        // Allow GL's shader compilation to use 2 background threads.
        //
        // Parallel compilation is documented to be enabled by default, but on
        // some drivers the parallel compilation does not actually activate
        // without explicitly setting this.
        //
        // NOTE: the spec states that apps may use "0xffffffff" to mean "use the
        // maximum number of threads", but that seems like an easy invitation
        // for a driver bug, so we are explicit about the number of threads.
        //
        // FIXME: When AsyncPipelineManager starts using >1 thread, we should
        // incorporate its same logic here.
        glMaxShaderCompilerThreadsKHR(2);
    }
#endif
}

void GLState::setScissor(IAABB scissor, uint32_t renderTargetHeight)
{
    assert(scissor.left >= 0);
    assert(scissor.right >= scissor.left);
    assert(scissor.top >= 0);
    assert(scissor.bottom >= scissor.top);
    setScissorRaw(scissor.left,
                  renderTargetHeight - scissor.bottom,
                  scissor.width(),
                  scissor.height());
}

void GLState::setScissorRaw(uint32_t left,
                            uint32_t top,
                            uint32_t width,
                            uint32_t height)
{
    if (!m_validState.scissorBox ||
        m_scissorBox != std::array<uint32_t, 4>{left, top, width, height})
    {
        glScissor(left, top, width, height);
        m_scissorBox = {left, top, width, height};
        m_validState.scissorBox = true;
    }

    if (!m_validState.scissorEnabled || !m_scissorEnabled)
    {
        glEnable(GL_SCISSOR_TEST);
        m_scissorEnabled = true;
        m_validState.scissorEnabled = true;
    }
}

void GLState::disableScissor()
{
    if (!m_validState.scissorEnabled || m_scissorEnabled)
    {
        glDisable(GL_SCISSOR_TEST);
        m_scissorEnabled = false;
        m_validState.scissorEnabled = true;
    }
}

static void gl_enable_disable(GLenum state, bool enabled)
{
    if (enabled)
        glEnable(state);
    else
        glDisable(state);
}

void GLState::setDepthStencilEnabled(bool depthEnabled, bool stencilEnabled)
{
    if (!m_validState.depthStencilEnabled || m_depthTestEnabled != depthEnabled)
    {
        gl_enable_disable(GL_DEPTH_TEST, depthEnabled);
        m_depthTestEnabled = depthEnabled;
    }

    if (!m_validState.depthStencilEnabled ||
        m_stencilTestEnabled != stencilEnabled)
    {
        gl_enable_disable(GL_STENCIL_TEST, stencilEnabled);
        m_stencilTestEnabled = stencilEnabled;
    }

    m_validState.depthStencilEnabled = true;
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

static GLenum gl_stencil_op(StencilOp op)
{
    switch (op)
    {
        case StencilOp::keep:
            return GL_KEEP;
        case StencilOp::replace:
            return GL_REPLACE;
        case StencilOp::zero:
            return GL_ZERO;
        case StencilOp::decrClamp:
            return GL_DECR;
        case StencilOp::incrWrap:
            return GL_INCR_WRAP;
        case StencilOp::decrWrap:
            return GL_DECR_WRAP;
    }
    RIVE_UNREACHABLE();
}

static GLenum gl_stencil_func(gpu::StencilCompareOp compareOp)
{
    switch (compareOp)
    {
        case gpu::StencilCompareOp::less:
            return GL_LESS;
        case gpu::StencilCompareOp::equal:
            return GL_EQUAL;
        case gpu::StencilCompareOp::lessOrEqual:
            return GL_LEQUAL;
        case gpu::StencilCompareOp::notEqual:
            return GL_NOTEQUAL;
        case gpu::StencilCompareOp::always:
            return GL_ALWAYS;
    }
    RIVE_UNREACHABLE();
}

static GLenum gl_cull_face(CullFace riveCullFace)
{
    switch (riveCullFace)
    {
        case CullFace::none:
            return GL_NONE;
        case CullFace::clockwise:
            return GL_FRONT;
        case CullFace::counterclockwise:
            return GL_BACK;
    }
    RIVE_UNREACHABLE();
}

void GLState::setBlendEquation(gpu::BlendEquation blendEquation)
{
    if (m_validState.blendEquation && blendEquation == m_blendEquation)
    {
        return;
    }
    if (!m_validState.blendEquation ||
        m_blendEquation == gpu::BlendEquation::none)
    {
        glEnable(GL_BLEND);
    }
    switch (blendEquation)
    {
        case gpu::BlendEquation::none:
            glDisable(GL_BLEND);
            break;
        case gpu::BlendEquation::srcOver:
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case gpu::BlendEquation::screen:
            glBlendEquation(GL_SCREEN_KHR);
            break;
        case gpu::BlendEquation::overlay:
            glBlendEquation(GL_OVERLAY_KHR);
            break;
        case gpu::BlendEquation::darken:
            glBlendEquation(GL_DARKEN_KHR);
            break;
        case gpu::BlendEquation::lighten:
            glBlendEquation(GL_LIGHTEN_KHR);
            break;
        case gpu::BlendEquation::colorDodge:
            glBlendEquation(GL_COLORDODGE_KHR);
            break;
        case gpu::BlendEquation::colorBurn:
            glBlendEquation(GL_COLORBURN_KHR);
            break;
        case gpu::BlendEquation::hardLight:
            glBlendEquation(GL_HARDLIGHT_KHR);
            break;
        case gpu::BlendEquation::softLight:
            glBlendEquation(GL_SOFTLIGHT_KHR);
            break;
        case gpu::BlendEquation::difference:
            glBlendEquation(GL_DIFFERENCE_KHR);
            break;
        case gpu::BlendEquation::exclusion:
            glBlendEquation(GL_EXCLUSION_KHR);
            break;
        case gpu::BlendEquation::multiply:
            glBlendEquation(GL_MULTIPLY_KHR);
            break;
        case gpu::BlendEquation::hue:
            glBlendEquation(GL_HSL_HUE_KHR);
            break;
        case gpu::BlendEquation::saturation:
            glBlendEquation(GL_HSL_SATURATION_KHR);
            break;
        case gpu::BlendEquation::color:
            glBlendEquation(GL_HSL_COLOR_KHR);
            break;
        case gpu::BlendEquation::luminosity:
            glBlendEquation(GL_HSL_LUMINOSITY_KHR);
            break;
        case gpu::BlendEquation::plus:
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_ONE, GL_ONE);
            break;
        case gpu::BlendEquation::max:
            glBlendEquation(GL_MAX);
            glBlendFunc(GL_ONE, GL_ONE);
            break;
    }
    m_blendEquation = blendEquation;
    m_validState.blendEquation = true;
}

void GLState::setWriteMasks(bool colorWriteMask,
                            bool depthWriteMask,
                            uint8_t stencilWriteMask)
{
    if (!m_validState.writeMasks)
    {
        glColorMask(colorWriteMask,
                    colorWriteMask,
                    colorWriteMask,
                    colorWriteMask);
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
            glColorMask(colorWriteMask,
                        colorWriteMask,
                        colorWriteMask,
                        colorWriteMask);
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

void GLState::setPipelineState(const gpu::PipelineState& pipelineState)
{
    disableScissor(); // Scissor isn't currently used in the pipeline state.
    setDepthStencilEnabled(pipelineState.depthTestEnabled,
                           pipelineState.stencilTestEnabled);
    if (pipelineState.stencilTestEnabled)
    {
        if (!pipelineState.stencilDoubleSided)
        {
            glStencilFunc(
                gl_stencil_func(pipelineState.stencilFrontOps.compareOp),
                pipelineState.stencilReference,
                pipelineState.stencilCompareMask);
            glStencilOp(
                gl_stencil_op(pipelineState.stencilFrontOps.failOp),
                gl_stencil_op(pipelineState.stencilFrontOps.depthFailOp),
                gl_stencil_op(pipelineState.stencilFrontOps.passOp));
        }
        else
        {
            glStencilFuncSeparate(
                GL_FRONT,
                gl_stencil_func(pipelineState.stencilFrontOps.compareOp),
                pipelineState.stencilReference,
                pipelineState.stencilCompareMask);
            glStencilOpSeparate(
                GL_FRONT,
                gl_stencil_op(pipelineState.stencilFrontOps.failOp),
                gl_stencil_op(pipelineState.stencilFrontOps.depthFailOp),
                gl_stencil_op(pipelineState.stencilFrontOps.passOp));
            glStencilFuncSeparate(
                GL_BACK,
                gl_stencil_func(pipelineState.stencilBackOps.compareOp),
                pipelineState.stencilReference,
                pipelineState.stencilCompareMask);
            glStencilOpSeparate(
                GL_BACK,
                gl_stencil_op(pipelineState.stencilBackOps.failOp),
                gl_stencil_op(pipelineState.stencilBackOps.depthFailOp),
                gl_stencil_op(pipelineState.stencilBackOps.passOp));
        }
    }
    setCullFace(gl_cull_face(pipelineState.cullFace));
    setBlendEquation(pipelineState.blendEquation);
    setWriteMasks(pipelineState.colorWriteEnabled,
                  pipelineState.depthWriteEnabled,
                  pipelineState.stencilWriteMask);
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
            // Don't track GL_ELEMENT_ARRAY_BUFFER, since it is tied to the VAO
            // state.
            glBindBuffer(target, bufferID);
            return;
        case GL_ARRAY_BUFFER:
            if (!m_validState.boundArrayBufferID ||
                bufferID != m_boundArrayBufferID)
            {
                glBindBuffer(GL_ARRAY_BUFFER, bufferID);
                m_boundArrayBufferID = bufferID;
                m_validState.boundArrayBufferID = true;
            }
            break;
        case GL_UNIFORM_BUFFER:
            if (!m_validState.boundUniformBufferID ||
                bufferID != m_boundUniformBufferID)
            {
                glBindBuffer(GL_UNIFORM_BUFFER, bufferID);
                m_boundUniformBufferID = bufferID;
                m_validState.boundUniformBufferID = true;
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
}
} // namespace rive::gpu

/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/renderer/gl/gles3.hpp"
#include "rive/refcnt.hpp"
#include "rive/renderer/gpu.hpp"

namespace rive::gpu
{
// Lightweight wrapper around common GL state.
class GLState : public RefCnt<GLState>
{
public:
    GLState(const GLCapabilities& capabilities) : m_capabilities(capabilities)
    {
        invalidate();
    }

    const GLCapabilities& capabilities() const { return m_capabilities; }

    void invalidate();

    void setDepthStencilEnabled(bool depthEnabled, bool stencilEnabled);
    void setCullFace(GLenum);
    void setBlendEquation(gpu::BlendEquation);
    void disableBlending() { setBlendEquation(gpu::BlendEquation::none); }
    void setWriteMasks(bool colorWriteMask,
                       bool depthWriteMask,
                       uint8_t stencilWriteMask);
    void setPipelineState(const gpu::PipelineState&);

    void bindProgram(GLuint);
    void bindVAO(GLuint);
    void bindBuffer(GLenum target, GLuint);

    void deleteProgram(GLuint);
    void deleteVAO(GLuint);
    void deleteBuffer(GLuint);

private:
    const GLCapabilities m_capabilities;
    gpu::BlendEquation m_blendEquation;
    bool m_depthTestEnabled;
    bool m_stencilTestEnabled;
    bool m_colorWriteMask;
    bool m_depthWriteMask;
    GLuint m_stencilWriteMask;
    GLenum m_cullFace;
    GLuint m_boundProgramID;
    GLuint m_boundVAO;
    GLuint m_boundArrayBufferID;
    GLuint m_boundUniformBufferID;

    struct
    {
        bool blendEquation : 1;
        bool depthStencilEnabled : 1;
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

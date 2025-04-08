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
    GLState(const GLCapabilities& capabilities) : m_capabilities(capabilities)
    {
        invalidate();
    }

    const GLCapabilities& capabilities() const { return m_capabilities; }

    void invalidate();

    enum class GLBlendMode
    {
        none = 0,

        srcOver = static_cast<int>(rive::BlendMode::srcOver),
        screen = static_cast<int>(rive::BlendMode::screen),
        overlay = static_cast<int>(rive::BlendMode::overlay),
        darken = static_cast<int>(rive::BlendMode::darken),
        lighten = static_cast<int>(rive::BlendMode::lighten),
        colorDodge = static_cast<int>(rive::BlendMode::colorDodge),
        colorBurn = static_cast<int>(rive::BlendMode::colorBurn),
        hardLight = static_cast<int>(rive::BlendMode::hardLight),
        softLight = static_cast<int>(rive::BlendMode::softLight),
        difference = static_cast<int>(rive::BlendMode::difference),
        exclusion = static_cast<int>(rive::BlendMode::exclusion),
        multiply = static_cast<int>(rive::BlendMode::multiply),
        hue = static_cast<int>(rive::BlendMode::hue),
        saturation = static_cast<int>(rive::BlendMode::saturation),
        color = static_cast<int>(rive::BlendMode::color),
        luminosity = static_cast<int>(rive::BlendMode::luminosity),

        plus = srcOver + 1,
        max = plus + 1,
    };
    void setGLBlendMode(GLBlendMode);
    void setBlendMode(rive::BlendMode blendMode)
    {
        setGLBlendMode(static_cast<GLBlendMode>(blendMode));
    }
    void disableBlending() { setGLBlendMode(GLBlendMode::none); }

    void setWriteMasks(bool colorWriteMask,
                       bool depthWriteMask,
                       GLuint stencilWriteMask);
    void setCullFace(GLenum);

    void bindProgram(GLuint);
    void bindVAO(GLuint);
    void bindBuffer(GLenum target, GLuint);

    void deleteProgram(GLuint);
    void deleteVAO(GLuint);
    void deleteBuffer(GLuint);

private:
    const GLCapabilities m_capabilities;
    GLBlendMode m_blendMode;
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

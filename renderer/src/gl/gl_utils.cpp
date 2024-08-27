/*
 * Copyright 2022 Rive
 */

#include "rive/renderer/gl/gl_utils.hpp"

#include <stdio.h>
#include <sstream>
#include <vector>

#include "generated/shaders/glsl.glsl.hpp"

#ifdef BYPASS_EMSCRIPTEN_SHADER_PARSER
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

// Emscripten's shader preprocessor crashes on PLS shaders. This method allows us to bypass
// Emscripten and set a WebGL shader source directly.
EM_JS(void,
      webgl_shader_source,
      (EMSCRIPTEN_WEBGL_CONTEXT_HANDLE gl, GLuint shader, const char* source),
      {
          gl = GL.getContext(gl).GLctx;
          shader = GL.shaders[shader];
          source = UTF8ToString(source);
          gl.shaderSource(shader, source);
      });
#endif

namespace glutils
{
void CompileAndAttachShader(GLuint program,
                            GLenum type,
                            const char* source,
                            const GLCapabilities& capabilities)
{
    CompileAndAttachShader(program, type, nullptr, 0, &source, 1, capabilities);
}

void CompileAndAttachShader(GLuint program,
                            GLenum type,
                            const char* defines[],
                            size_t numDefines,
                            const char* inputSources[],
                            size_t numInputSources,
                            const GLCapabilities& capabilities)
{
    GLuint shader =
        CompileShader(type, defines, numDefines, inputSources, numInputSources, capabilities);
    glAttachShader(program, shader);
    glDeleteShader(shader);
}

GLuint CompileShader(GLuint type, const char* source, const GLCapabilities& capabilities)
{
    return CompileShader(type, nullptr, 0, &source, 1, capabilities);
}

GLuint CompileShader(GLuint type,
                     const char* defines[],
                     size_t numDefines,
                     const char* inputSources[],
                     size_t numInputSources,
                     const GLCapabilities& capabilities)
{
    std::ostringstream shaderSource;
    shaderSource << "#version " << capabilities.contextVersionMajor
                 << capabilities.contextVersionMinor << '0';
    if (capabilities.isGLES)
    {
        shaderSource << " es";
    }
    shaderSource << '\n';
    // Create our own "GLSL_VERSION" macro. In "#version 320 es", Qualcomm incorrectly substitutes
    // __VERSION__ to 300.
    shaderSource << "#define " << GLSL_GLSL_VERSION << ' ' << capabilities.contextVersionMajor
                 << capabilities.contextVersionMinor << "0\n";
    if (type == GL_VERTEX_SHADER)
    {
        shaderSource << "#define " << GLSL_VERTEX "\n";
    }
    else if (GL_FRAGMENT_SHADER)
    {
        shaderSource << "#define " << GLSL_FRAGMENT "\n";
    }
    for (size_t i = 0; i < numDefines; ++i)
    {
        shaderSource << "#define " << defines[i] << " true\n";
    }
    shaderSource << rive::gpu::glsl::glsl << "\n";
    for (size_t i = 0; i < numInputSources; ++i)
    {
        shaderSource << inputSources[i] << "\n";
    }
    return CompileRawGLSL(type, shaderSource.str().c_str());
}

[[nodiscard]] GLuint CompileRawGLSL(GLuint shaderType, const char* rawGLSL)
{
    GLuint shader = glCreateShader(shaderType);
#ifdef BYPASS_EMSCRIPTEN_SHADER_PARSER
    // Emscripten's shader preprocessor crashes on PLS shaders. Feed Emscripten something very
    // simple and then hop to WebGL to bypass it and set the real shader source.
    const char* kMinimalShader = shaderType == GL_VERTEX_SHADER
                                     ? "#version 300 es\nvoid main() { gl_Position = vec4(0); }"
                                     : "#version 300 es\nvoid main() {}";
    glShaderSource(shader, 1, &kMinimalShader, nullptr);
    webgl_shader_source(emscripten_webgl_get_current_context(), shader, rawGLSL);
#else
    glShaderSource(shader, 1, &rawGLSL, nullptr);
#endif
    glCompileShader(shader);
#ifdef DEBUG
    GLint isCompiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE)
    {
        GLint maxLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
        std::vector<GLchar> infoLog(maxLength);
        glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);
        fprintf(stderr, "Failed to compile shader\n");
        int l = 1;
        std::stringstream stream(rawGLSL);
        std::string lineStr;
        while (std::getline(stream, lineStr, '\n'))
        {
            fprintf(stderr, "%4i| %s\n", l++, lineStr.c_str());
        }
        fprintf(stderr, "%s\n", &infoLog[0]);
        fflush(stderr);
        glDeleteShader(shader);
        abort();
    }
#endif
    return shader;
}

void LinkProgram(GLuint program)
{
    glLinkProgram(program);
#ifdef DEBUG
    GLint isLinked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
    if (isLinked == GL_FALSE)
    {
        GLint maxLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
        std::vector<GLchar> infoLog(maxLength);
        glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);
        fprintf(stderr, "Failed to link program %s\n", &infoLog[0]);
        fflush(stderr);
        abort();
    }
#endif
}

void Program::reset(GLuint adoptedProgramID)
{
    if (m_fragmentShaderID != 0)
    {
        glDeleteShader(m_fragmentShaderID);
        m_fragmentShaderID = 0;
    }
    if (m_vertexShaderID != 0)
    {
        glDeleteShader(m_vertexShaderID);
        m_vertexShaderID = 0;
    }
    if (m_id != 0)
    {
        glDeleteProgram(m_id);
    }
    m_id = adoptedProgramID;
}

void Program::compileAndAttachShader(GLuint type,
                                     const char* defines[],
                                     size_t numDefines,
                                     const char* sources[],
                                     size_t numSources,
                                     const GLCapabilities& capabilities)
{
    assert(type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER);
    GLuint& internalShaderID = type == GL_VERTEX_SHADER ? m_vertexShaderID : m_fragmentShaderID;
    if (internalShaderID != 0)
    {
        glDeleteShader(internalShaderID);
    }
    internalShaderID = CompileShader(type, defines, numDefines, sources, numSources, capabilities);
    glAttachShader(m_id, internalShaderID);
}

void SetTexture2DSamplingParams(GLenum minFilter, GLenum magFilter)
{
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void BlitFramebuffer(rive::IAABB bounds, uint32_t renderTargetHeight, GLbitfield mask)
{
    // glBlitFramebuffer is oriented bottom-up.
    uint32_t l = bounds.left;
    uint32_t b = renderTargetHeight - bounds.bottom;
    uint32_t r = bounds.right;
    uint32_t t = renderTargetHeight - bounds.top;
    glBlitFramebuffer(l, b, r, t, l, b, r, t, mask, GL_NEAREST);
}
} // namespace glutils

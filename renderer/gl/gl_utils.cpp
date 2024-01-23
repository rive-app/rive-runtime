/*
 * Copyright 2022 Rive
 */

#include "gl_utils.hpp"

#include <stdio.h>
#include <sstream>
#include <vector>

#include "../out/obj/generated/glsl.glsl.hpp"

namespace glutils
{
void CompileAndAttachShader(GLuint program,
                            GLuint type,
                            const char* source,
                            const GLCapabilities& capabilities)
{
    CompileAndAttachShader(program, type, nullptr, 0, &source, 1, capabilities);
}

void CompileAndAttachShader(GLuint program,
                            GLuint type,
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
        shaderSource << "#define " << defines[i] << "\n";
    }
    shaderSource << rive::pls::glsl::glsl << "\n";
    for (size_t i = 0; i < numInputSources; ++i)
    {
        shaderSource << inputSources[i] << "\n";
    }
    return CompileRawGLSL(type, shaderSource.str().c_str());
}

[[nodiscard]] GLuint CompileRawGLSL(GLuint shaderType, const char* rawGLSL)
{
    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &rawGLSL, nullptr);
    glCompileShader(shader);
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
        exit(-1);
    }
    return shader;
}

void LinkProgram(GLuint program)
{
    glLinkProgram(program);
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
        exit(-1);
    }
}

void BlitFramebuffer(rive::IAABB bounds, uint32_t renderTargetHeight, GLbitfield mask)
{
    // glBlitFramebuffer is oriented bottom-up.
    uint32_t x = bounds.left;
    uint32_t y = renderTargetHeight - bounds.bottom;
    uint32_t w = bounds.width();
    uint32_t h = bounds.height();
    glBlitFramebuffer(x, y, w, h, x, y, w, h, mask, GL_NEAREST);
}
} // namespace glutils

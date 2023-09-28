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
                            const char* versionString)
{
    CompileAndAttachShader(program, type, nullptr, 0, &source, 1, versionString);
}

void CompileAndAttachShader(GLuint program,
                            GLuint type,
                            const char* defines[],
                            size_t numDefines,
                            const char* inputSources[],
                            size_t numInputSources,
                            const char* versionString)
{
    GLuint shader = CompileShader(type,
                                  defines,
                                  numDefines,
                                  inputSources,
                                  numInputSources,
                                  versionString);
    glAttachShader(program, shader);
    glDeleteShader(shader);
}

GLuint CompileShader(GLuint type, const char* source, const char* versionString)
{
    return CompileShader(type, nullptr, 0, &source, 1, versionString);
}

GLuint CompileShader(GLuint type,
                     const char* defines[],
                     size_t numDefines,
                     const char* inputSources[],
                     size_t numInputSources,
                     const char* versionString)
{
    std::ostringstream shaderSource;
    if (versionString != nullptr)
    {
        shaderSource << versionString;
    }
    else
    {
        shaderSource << "#version 300 es\n";
    }
    if (type == GL_VERTEX_SHADER)
    {
        shaderSource << "#define " GLSL_VERTEX "\n";
    }
    else if (GL_FRAGMENT_SHADER)
    {
        shaderSource << "#define " GLSL_FRAGMENT "\n";
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
    const std::string& shaderSourceStr = shaderSource.str();
    const char* shaderSourceCStr = shaderSourceStr.c_str();
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &shaderSourceCStr, nullptr);
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
        std::stringstream stream(shaderSourceCStr);
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
} // namespace glutils

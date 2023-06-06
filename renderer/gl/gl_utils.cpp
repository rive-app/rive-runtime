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
void CompileAndAttachShader(GLuint program, GLuint type, const char* source)
{
    CompileAndAttachShader(program, type, nullptr, 0, &source, 1);
}

void CompileAndAttachShader(GLuint program,
                            GLuint type,
                            const char* defines[],
                            size_t numDefines,
                            const char* inputSources[],
                            size_t numInputSources)
{
    GLuint shader = CompileShader(type, defines, numDefines, inputSources, numInputSources);
    glAttachShader(program, shader);
    glDeleteShader(shader);
}

GLuint CompileShader(GLuint type, const char* source)
{
    return CompileShader(type, nullptr, 0, &source, 1);
}

GLuint CompileShader(GLuint type,
                     const char* defines[],
                     size_t numDefines,
                     const char* inputSources[],
                     size_t numInputSources)
{
    std::vector<const char*> sources;
    sources.push_back("#version 300 es\n");
    if (type == GL_VERTEX_SHADER)
    {
        sources.push_back("#define " GLSL_VERTEX "\n");
    }
    else if (GL_FRAGMENT_SHADER)
    {
        sources.push_back("#define " GLSL_FRAGMENT "\n");
    }
    std::ostringstream definesStream;
    for (size_t i = 0; i < numDefines; ++i)
    {
        definesStream << "#define " << defines[i] << "\n";
    }
    std::string definesString;
    if (numDefines > 0)
    {
        definesString = definesStream.str();
        sources.push_back(definesString.c_str());
    }
    sources.push_back(rive::pls::glsl::glsl);
    for (size_t i = 0; i < numInputSources; ++i)
    {
        sources.push_back(inputSources[i]);
    }
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, sources.size(), sources.data(), nullptr);
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
        for (const char* s : sources)
        {
            std::stringstream stream(s);
            std::string lineStr;
            while (std::getline(stream, lineStr, '\n'))
            {
                fprintf(stderr, "%4i| %s\n", l++, lineStr.c_str());
            }
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

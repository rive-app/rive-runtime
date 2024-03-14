/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/pls/gl/gles3.hpp"
#include "rive/math/aabb.hpp"
#include <cstddef>

namespace glutils
{
void CompileAndAttachShader(GLuint program, GLenum type, const char* source, const GLCapabilities&);

void CompileAndAttachShader(GLuint program,
                            GLenum type,
                            const char* defines[],
                            size_t numDefines,
                            const char* sources[],
                            size_t numSources,
                            const GLCapabilities&);

[[nodiscard]] GLuint CompileShader(GLuint type, const char* source, const GLCapabilities&);

[[nodiscard]] GLuint CompileShader(GLuint type,
                                   const char* defines[],
                                   size_t numDefines,
                                   const char* sources[],
                                   size_t numSources,
                                   const GLCapabilities&);

[[nodiscard]] GLuint CompileRawGLSL(GLuint shaderType, const char* rawGLSL);

void LinkProgram(GLuint program);

class Buffer
{
public:
    Buffer() { glGenBuffers(1, &m_id); }
    ~Buffer() { glDeleteBuffers(1, &m_id); }
    operator GLuint() const { return m_id; }

private:
    GLuint m_id;
};

class FBO
{
public:
    FBO() { glGenFramebuffers(1, &m_id); }
    ~FBO() { glDeleteFramebuffers(1, &m_id); }
    operator GLuint() const { return m_id; }

private:
    GLuint m_id;
};

class VAO
{
public:
    VAO() { glGenVertexArrays(1, &m_id); }
    ~VAO() { glDeleteVertexArrays(1, &m_id); }
    operator GLuint() const { return m_id; }

private:
    GLuint m_id;
};

class Program
{
public:
    Program(const Program&) = delete;
    Program& operator=(const Program&) = delete;

    Program(GLuint adoptedProgramID) : m_programID(adoptedProgramID) {}
    Program() : Program(glCreateProgram()) {}
    ~Program() { reset(0); }

    operator GLuint() const { return m_programID; }

    void reset(GLuint adoptedProgramID);

    void compileAndAttachShader(GLenum type, const char* source, const GLCapabilities& capabilities)
    {
        compileAndAttachShader(type, nullptr, 0, &source, 1, capabilities);
    }
    void compileAndAttachShader(GLenum type,
                                const char* defines[],
                                size_t numDefines,
                                const char* sources[],
                                size_t numSources,
                                const GLCapabilities&);

    void link() { LinkProgram(m_programID); }

private:
    GLuint m_programID = glCreateProgram();
    GLuint m_vertexShaderID = 0;
    GLuint m_fragmentShaderID = 0;
};

void SetTexture2DSamplingParams(GLenum minFilter, GLenum magFilter);

void BlitFramebuffer(rive::IAABB bounds,
                     uint32_t renderTargetHeight,
                     GLbitfield mask = GL_COLOR_BUFFER_BIT);
} // namespace glutils

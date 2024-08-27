/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/renderer/gl/gles3.hpp"
#include "rive/math/aabb.hpp"
#include <cstddef>
#include <utility>

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

class GLObject
{
public:
    GLObject() = default;
    GLObject(GLObject&& rhs) : m_id(std::exchange(rhs.m_id, 0)) {}

    GLObject(const GLObject&) = delete;
    GLObject& operator=(const GLObject&) = delete;

    operator GLuint() const { return m_id; }

protected:
    explicit GLObject(GLuint adoptedID) : m_id(adoptedID) {}

    GLuint m_id = 0;
};

class Buffer : public GLObject
{
public:
    Buffer() { glGenBuffers(1, &m_id); }
    ~Buffer() { glDeleteBuffers(1, &m_id); }
};

class Texture : public GLObject
{
public:
    Texture() { glGenTextures(1, &m_id); }
    Texture(Texture&& rhs) : GLObject(std::move(rhs)) {}
    Texture& operator=(Texture&& rhs)
    {
        reset(std::exchange(rhs.m_id, 0));
        return *this;
    }
    ~Texture() { reset(0); }

    static Texture Zero() { return Texture(0); }

private:
    explicit Texture(GLuint adoptedID) : GLObject(adoptedID) {}

    void reset(GLuint adoptedID)
    {
        if (m_id != 0)
        {
            glDeleteTextures(1, &m_id);
        }
        m_id = adoptedID;
    }
};

class Framebuffer : public GLObject
{
public:
    Framebuffer() { glGenFramebuffers(1, &m_id); }
    Framebuffer(Framebuffer&& rhs) : GLObject(std::move(rhs)) {}
    Framebuffer& operator=(Framebuffer&& rhs)
    {
        reset(std::exchange(rhs.m_id, 0));
        return *this;
    }
    ~Framebuffer() { reset(0); }

    static Framebuffer Zero() { return Framebuffer(0); }

private:
    explicit Framebuffer(GLuint adoptedID) : GLObject(adoptedID) {}

    void reset(GLuint adoptedID)
    {
        if (m_id != 0)
        {
            glDeleteFramebuffers(1, &m_id);
        }
        m_id = adoptedID;
    }
};

class Renderbuffer : public GLObject
{
public:
    Renderbuffer() { glGenRenderbuffers(1, &m_id); }
    Renderbuffer(Renderbuffer&& rhs) : GLObject(std::move(rhs)) {}
    Renderbuffer& operator=(Renderbuffer&& rhs)
    {
        reset(std::exchange(rhs.m_id, 0));
        return *this;
    }
    ~Renderbuffer() { reset(0); }

    static Renderbuffer Zero() { return Renderbuffer(0); }

private:
    explicit Renderbuffer(GLuint adoptedID) : GLObject(adoptedID) {}

    void reset(GLuint adoptedID)
    {
        if (m_id != 0)
        {
            glDeleteRenderbuffers(1, &m_id);
        }
        m_id = adoptedID;
    }
};

class VAO : public GLObject
{
public:
    VAO() { glGenVertexArrays(1, &m_id); }
    ~VAO() { glDeleteVertexArrays(1, &m_id); }
};

class Program : public GLObject
{
public:
    Program() : GLObject(glCreateProgram()) {}
    Program& operator=(Program&& rhs)
    {
        reset(std::exchange(rhs.m_id, 0));
        m_vertexShaderID = std::exchange(rhs.m_vertexShaderID, 0);
        m_fragmentShaderID = std::exchange(rhs.m_fragmentShaderID, 0);
        return *this;
    }
    ~Program() { reset(0); }

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

    void link() { LinkProgram(m_id); }

    static Program Zero() { return Program(0); }

private:
    explicit Program(GLuint adoptedID) : GLObject(adoptedID) {}

    void reset(GLuint adoptedProgramID);

    GLuint m_vertexShaderID = 0;
    GLuint m_fragmentShaderID = 0;
};

void SetTexture2DSamplingParams(GLenum minFilter, GLenum magFilter);

void BlitFramebuffer(rive::IAABB bounds,
                     uint32_t renderTargetHeight,
                     GLbitfield mask = GL_COLOR_BUFFER_BIT);
} // namespace glutils

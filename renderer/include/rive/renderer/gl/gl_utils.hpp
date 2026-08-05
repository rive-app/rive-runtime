/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/renderer/gl/gles3.hpp"
#include "rive/math/aabb.hpp"
#include "rive/shapes/paint/image_sampler.hpp"
#include <cstddef>
#include <utility>

namespace glutils
{
// Used when the driver doesn't support gl_BaseInstance
// (GLCapabilities::ANGLE_base_vertex_base_instance_shader_builtin is false).
//
// The client must set this uniform value before drawing if the shader needs an
// instance index.
//
// (Begin the variable name with an underscore so it won't collide with any
// renames from minify.py.)
constexpr static char BASE_INSTANCE_UNIFORM_NAME[] = "_baseInstance";

#ifdef DEBUG
void PrintShaderCompilationErrors(GLuint shader);
void PrintLinkProgramErrors(GLuint program);
#endif

enum class DebugPrintErrorAndAbort
{
    no,
    yes,
};

void CompileAndAttachShader(
    GLuint program,
    GLenum type,
    const char* source,
    const GLCapabilities&,
    DebugPrintErrorAndAbort = DebugPrintErrorAndAbort::yes);

void CompileAndAttachShader(
    GLuint program,
    GLenum type,
    const char* defines[],
    size_t numDefines,
    const char* sources[],
    size_t numSources,
    const GLCapabilities&,
    DebugPrintErrorAndAbort = DebugPrintErrorAndAbort::yes);

[[nodiscard]] GLuint CompileShader(
    GLuint type,
    const char* source,
    const GLCapabilities&,
    DebugPrintErrorAndAbort = DebugPrintErrorAndAbort::yes);

[[nodiscard]] GLuint CompileShader(
    GLuint type,
    const char* defines[],
    size_t numDefines,
    const char* sources[],
    size_t numSources,
    const GLCapabilities&,
    DebugPrintErrorAndAbort = DebugPrintErrorAndAbort::yes);

[[nodiscard]] GLuint CompileRawGLSL(
    GLenum shaderType,
    const char* rawGLSL,
    DebugPrintErrorAndAbort = DebugPrintErrorAndAbort::yes);

void LinkProgram(GLuint program,
                 DebugPrintErrorAndAbort = DebugPrintErrorAndAbort::yes);

// Threaded wasm reaches one heap from several contexts, and a GL name means
// nothing outside the context that made it: every context numbers from 1.
#ifdef __EMSCRIPTEN_PTHREADS__
#define RIVE_GL_NAMES_ARE_PER_CONTEXT
#endif

enum class GLObjectType
{
    buffer,
    texture,
    framebuffer,
    renderbuffer,
    vertexArray,
    shader,
    program,
};

using GLContextID = int;

#ifdef RIVE_GL_NAMES_ARE_PER_CONTEXT
GLContextID CurrentContextID();

// Deletes the names other threads left behind for whichever context is current.
void ReclaimAbandonedNames();

// Process wide, for tests: names left to their owning context, and names that
// owner has since deleted.
uint32_t AbandonedNameCount();
uint32_t ReclaimedNameCount();
#else
// A process with a single context can always delete what it created.
constexpr GLContextID CurrentContextID() { return 0; }
inline void ReclaimAbandonedNames() {}
#endif

class GLObject
{
public:
    GLObject() = default;
    GLObject(GLObject&& rhs) :
        m_id(std::exchange(rhs.m_id, 0))
#ifdef RIVE_GL_NAMES_ARE_PER_CONTEXT
        ,
        m_context(rhs.m_context)
#endif
    {}

    GLObject(const GLObject&) = delete;
    GLObject& operator=(const GLObject&) = delete;

    operator GLuint() const { return m_id; }

protected:
    explicit GLObject(GLuint adoptedID) : m_id(adoptedID) {}

    // Deletes m_id, on the context that created it.
    void destroy(GLObjectType);
    // Deletes m_id and takes over rhs's name and the context it belongs to.
    void adopt(GLObjectType, GLObject&& rhs);
    // Deletes m_id and takes over a name generated on the current context.
    void adoptName(GLObjectType, GLuint adoptedID);

    GLuint m_id = 0;
#ifdef RIVE_GL_NAMES_ARE_PER_CONTEXT
    GLContextID m_context = CurrentContextID();
#endif
};

class Buffer : public GLObject
{
public:
    Buffer() { glGenBuffers(1, &m_id); }
    ~Buffer() { destroy(GLObjectType::buffer); }
};

class Texture : public GLObject
{
public:
    Texture() { glGenTextures(1, &m_id); }
    Texture(Texture&& rhs) : GLObject(std::move(rhs)) {}
    Texture& operator=(Texture&& rhs)
    {
        adopt(GLObjectType::texture, std::move(rhs));
        return *this;
    }
    ~Texture() { destroy(GLObjectType::texture); }

    static Texture Zero() { return Texture(0); }
    static Texture Adopt(GLuint id) { return Texture(id); }

private:
    explicit Texture(GLuint adoptedID) : GLObject(adoptedID) {}
};

class Framebuffer : public GLObject
{
public:
    Framebuffer() { glGenFramebuffers(1, &m_id); }
    Framebuffer(Framebuffer&& rhs) : GLObject(std::move(rhs)) {}
    Framebuffer& operator=(Framebuffer&& rhs)
    {
        adopt(GLObjectType::framebuffer, std::move(rhs));
        return *this;
    }
    ~Framebuffer() { destroy(GLObjectType::framebuffer); }

    static Framebuffer Zero() { return Framebuffer(0); }

private:
    explicit Framebuffer(GLuint adoptedID) : GLObject(adoptedID) {}
};

class Renderbuffer : public GLObject
{
public:
    Renderbuffer() { glGenRenderbuffers(1, &m_id); }
    Renderbuffer(Renderbuffer&& rhs) : GLObject(std::move(rhs)) {}
    Renderbuffer& operator=(Renderbuffer&& rhs)
    {
        adopt(GLObjectType::renderbuffer, std::move(rhs));
        return *this;
    }
    ~Renderbuffer() { destroy(GLObjectType::renderbuffer); }

    static Renderbuffer Zero() { return Renderbuffer(0); }

private:
    explicit Renderbuffer(GLuint adoptedID) : GLObject(adoptedID) {}
};

class VAO : public GLObject
{
public:
    VAO() { glGenVertexArrays(1, &m_id); }
    ~VAO() { destroy(GLObjectType::vertexArray); }
};

class Shader : public GLObject
{
public:
    Shader() = default;
    Shader(Shader&& rhs) : GLObject(std::move(rhs)) {}
    Shader& operator=(Shader&& rhs)
    {
        adopt(GLObjectType::shader, std::move(rhs));
        return *this;
    }
    ~Shader() { destroy(GLObjectType::shader); }

    void compile(GLenum type,
                 const char* source,
                 const GLCapabilities& capabilities)
    {
        compile(type, nullptr, 0, &source, 1, capabilities);
    }
    void compile(GLenum type,
                 const char* defines[],
                 size_t numDefines,
                 const char* sources[],
                 size_t numSources,
                 const GLCapabilities&);

    void reset(GLuint adoptedID = 0)
    {
        adoptName(GLObjectType::shader, adoptedID);
    }
};

class Program : public GLObject
{
public:
    Program() : GLObject(glCreateProgram()) {}
    Program& operator=(Program&& rhs)
    {
        adopt(GLObjectType::program, std::move(rhs));
        m_vertexShader = std::move(rhs.m_vertexShader);
        m_fragmentShader = std::move(rhs.m_fragmentShader);
        return *this;
    }
    ~Program() { destroy(GLObjectType::program); }

    void compileAndAttachShader(GLenum type,
                                const char* source,
                                const GLCapabilities& capabilities)
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

    glutils::Shader m_vertexShader;
    glutils::Shader m_fragmentShader;
};

void SetTexture2DSamplingParams(GLenum minFilter, GLenum magFilter);
void SetTexture2DSamplingParams(rive::ImageSampler);

void BlitFramebuffer(rive::IAABB bounds,
                     uint32_t renderTargetHeight,
                     GLbitfield mask = GL_COLOR_BUFFER_BIT);

void Uniform1iByName(GLuint programID, const char* name, GLint value);
} // namespace glutils

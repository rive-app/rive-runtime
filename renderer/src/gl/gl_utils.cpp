/*
 * Copyright 2022 Rive
 */

#include "rive/renderer/gl/gl_utils.hpp"
#include "rive/shapes/paint/image_sampler.hpp"

#include <stdio.h>
#include <sstream>
#include <thread>
#include <vector>

#include "generated/shaders/glsl.glsl.hpp"

#ifdef BYPASS_EMSCRIPTEN_SHADER_PARSER
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

// Emscripten's shader preprocessor crashes on PLS shaders. This method allows
// us to bypass Emscripten and set a WebGL shader source directly.
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
                            const GLCapabilities& capabilities,
                            DebugPrintErrorAndAbort debugPrintErrornAndAbort)
{
    CompileAndAttachShader(program,
                           type,
                           nullptr,
                           0,
                           &source,
                           1,
                           capabilities,
                           debugPrintErrornAndAbort);
}

void CompileAndAttachShader(GLuint program,
                            GLenum type,
                            const char* defines[],
                            size_t numDefines,
                            const char* inputSources[],
                            size_t numInputSources,
                            const GLCapabilities& capabilities,
                            DebugPrintErrorAndAbort debugPrintErrornAndAbort)
{
    GLuint shader = CompileShader(type,
                                  defines,
                                  numDefines,
                                  inputSources,
                                  numInputSources,
                                  capabilities,
                                  debugPrintErrornAndAbort);
    glAttachShader(program, shader);
    glDeleteShader(shader);
}

GLuint CompileShader(GLuint type,
                     const char* source,
                     const GLCapabilities& capabilities,
                     DebugPrintErrorAndAbort debugPrintErrornAndAbort)
{
    return CompileShader(type,
                         nullptr,
                         0,
                         &source,
                         1,
                         capabilities,
                         debugPrintErrornAndAbort);
}

GLuint CompileShader(GLuint type,
                     const char* defines[],
                     size_t numDefines,
                     const char* inputSources[],
                     size_t numInputSources,
                     const GLCapabilities& capabilities,
                     DebugPrintErrorAndAbort debugPrintErrornAndAbort)
{
    std::ostringstream shaderSource;
    shaderSource << "#version " << capabilities.contextVersionMajor
                 << capabilities.contextVersionMinor << '0';
    if (capabilities.isGLES)
    {
        shaderSource << " es";
    }
    shaderSource << '\n';
    // Create our own "GLSL_VERSION" macro. In "#version 320 es", Qualcomm
    // incorrectly substitutes
    // __VERSION__ to 300.
    shaderSource << "#define " << GLSL_GLSL_VERSION << ' '
                 << capabilities.contextVersionMajor
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
    if (!capabilities.ANGLE_base_vertex_base_instance_shader_builtin)
    {
        shaderSource << "#define " << GLSL_BASE_INSTANCE_UNIFORM_NAME << ' '
                     << BASE_INSTANCE_UNIFORM_NAME << '\n';
    }
    if (capabilities.needsFloatingPointTessellationTexture)
    {
        shaderSource << "#define " << GLSL_TESS_TEXTURE_FLOATING_POINT << '\n';
    }
    shaderSource << rive::gpu::glsl::glsl << "\n";
    for (size_t i = 0; i < numInputSources; ++i)
    {
        shaderSource << inputSources[i] << "\n";
    }
    return CompileRawGLSL(type,
                          shaderSource.str().c_str(),
                          debugPrintErrornAndAbort);
}

#ifdef DEBUG
void PrintShaderCompilationErrors(GLuint shader)
{
    GLint maxLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
    std::vector<GLchar> infoLog(maxLength);
    glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);
    fprintf(stderr, "Failed to compile shader\n");
    // Print the error message *before* the shader in case stderr hasn't
    // finished flushing when we call abort() further on.
    fprintf(stderr, "%s\n", &infoLog[0]);

    GLint sourceLength = 0;
    glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &sourceLength);
    std::vector<GLchar> shaderSource(sourceLength);
    glGetShaderSource(shader, sourceLength, nullptr, shaderSource.data());
    int l = 1;
    std::stringstream stream(shaderSource.data());
    std::string lineStr;
    while (std::getline(stream, lineStr, '\n'))
    {
        fprintf(stderr, "%4i| %s\n", l++, lineStr.c_str());
    }
    // Print the error message, again, *after* the shader where it's easier
    // to find in the console.
    fprintf(stderr, "%s\n", &infoLog[0]);
    fflush(stderr);
}
#endif

[[nodiscard]] GLuint CompileRawGLSL(
    GLenum shaderType,
    const char* rawGLSL,
    DebugPrintErrorAndAbort debugPrintErrornAndAbort)
{
    GLuint shader = glCreateShader(shaderType);
#ifdef BYPASS_EMSCRIPTEN_SHADER_PARSER
    // Emscripten's shader preprocessor crashes on PLS shaders. Feed Emscripten
    // something very simple and then hop to WebGL to bypass it and set the real
    // shader source.
    const char* kMinimalShader =
        shaderType == GL_VERTEX_SHADER
            ? "#version 300 es\nvoid main() { gl_Position = vec4(0); }"
            : "#version 300 es\nvoid main() {}";
    glShaderSource(shader, 1, &kMinimalShader, nullptr);
    webgl_shader_source(emscripten_webgl_get_current_context(),
                        shader,
                        rawGLSL);
#else
    glShaderSource(shader, 1, &rawGLSL, nullptr);
#endif
    glCompileShader(shader);
#ifdef DEBUG
    if (debugPrintErrornAndAbort == DebugPrintErrorAndAbort::yes)
    {
        GLint isCompiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
        if (isCompiled == GL_FALSE)
        {
            PrintShaderCompilationErrors(shader);
            glDeleteShader(shader);
            // Give stderr another second to finish flushing before we abort.
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            abort();
        }
    }
#else
    std::ignore = debugPrintErrornAndAbort;
#endif
    return shader;
}

#ifdef DEBUG
void PrintLinkProgramErrors(GLuint program)
{
    GLint maxLength = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
    std::vector<GLchar> infoLog(maxLength);
    glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);
    fprintf(stderr, "Failed to link program %s\n", &infoLog[0]);
    fflush(stderr);
}
#endif

void LinkProgram(GLuint program,
                 DebugPrintErrorAndAbort debugPrintErrornAndAbort)
{
    glLinkProgram(program);
#ifdef DEBUG
    if (debugPrintErrornAndAbort == DebugPrintErrorAndAbort::yes)
    {
        GLint isLinked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
        if (isLinked == GL_FALSE)
        {
            PrintLinkProgramErrors(program);
            abort();
        }
    }
#else
    std::ignore = debugPrintErrornAndAbort;
#endif
}

void Program::reset(GLuint adoptedProgramID)
{
    m_fragmentShader.reset();
    m_vertexShader.reset();
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
    glutils::Shader& internalShader =
        type == GL_VERTEX_SHADER ? m_vertexShader : m_fragmentShader;
    internalShader
        .compile(type, defines, numDefines, sources, numSources, capabilities);
    glAttachShader(m_id, internalShader);
}

void Shader::compile(GLenum type,
                     const char* defines[],
                     size_t numDefines,
                     const char* sources[],
                     size_t numSources,
                     const GLCapabilities& capabilities)
{
    reset(CompileShader(type,
                        defines,
                        numDefines,
                        sources,
                        numSources,
                        capabilities));
}

void SetTexture2DSamplingParams(GLenum minFilter, GLenum magFilter)
{
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

GLint gl_wrap_from_image_wrap(rive::ImageWrap wrap)
{
    switch (wrap)
    {
        case rive::ImageWrap::clamp:
            return GL_CLAMP_TO_EDGE;
        case rive::ImageWrap::repeat:
            return GL_REPEAT;
        case rive::ImageWrap::mirror:
            return GL_MIRRORED_REPEAT;
    }

    RIVE_UNREACHABLE();
}

GLint gl_min_filter_for_image_filter(rive::ImageFilter filter)
{
    switch (filter)
    {
        case rive::ImageFilter::bilinear:
            return GL_LINEAR_MIPMAP_NEAREST;
        case rive::ImageFilter::nearest:
            return GL_NEAREST;
    }

    RIVE_UNREACHABLE();
}

GLint gl_mag_filter_for_image_filter(rive::ImageFilter filter)
{
    switch (filter)
    {
        case rive::ImageFilter::nearest:
            return GL_NEAREST;
        case rive::ImageFilter::bilinear:
            return GL_LINEAR;
    }

    RIVE_UNREACHABLE();
}

void SetTexture2DSamplingParams(rive::ImageSampler samplingParams)
{
    glTexParameteri(GL_TEXTURE_2D,
                    GL_TEXTURE_MIN_FILTER,
                    gl_min_filter_for_image_filter(samplingParams.filter));
    glTexParameteri(GL_TEXTURE_2D,
                    GL_TEXTURE_MAG_FILTER,
                    gl_mag_filter_for_image_filter(samplingParams.filter));
    glTexParameteri(GL_TEXTURE_2D,
                    GL_TEXTURE_WRAP_S,
                    gl_wrap_from_image_wrap(samplingParams.wrapX));
    glTexParameteri(GL_TEXTURE_2D,
                    GL_TEXTURE_WRAP_T,
                    gl_wrap_from_image_wrap(samplingParams.wrapY));
}

void BlitFramebuffer(rive::IAABB bounds,
                     uint32_t renderTargetHeight,
                     GLbitfield mask)
{
    // glBlitFramebuffer is oriented bottom-up.
    uint32_t l = bounds.left;
    uint32_t b = renderTargetHeight - bounds.bottom;
    uint32_t r = bounds.right;
    uint32_t t = renderTargetHeight - bounds.top;
    glBlitFramebuffer(l, b, r, t, l, b, r, t, mask, GL_NEAREST);
}

void Uniform1iByName(GLuint programID, const char* name, GLint value)
{
    GLint location = glGetUniformLocation(programID, name);
    // Don't allow non-existent uniforms. glUniform1i() is supposed to silently
    // ignore -1, but Moto G7 Play throws an error. We also just shouldn't be
    // querying uniform locations we know aren't going to exist anyway for
    // performance reasons.
    assert(location != -1);
    glUniform1i(location, value);
}

// Setup a small test to verify that GL_PIXEL_LOCAL_FORMAT_ANGLE has the correct
// value.
bool validate_pixel_local_storage_angle()
{
#if defined(RIVE_DESKTOP_GL) || defined(RIVE_WEBGL)
    glutils::Texture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, 1, 1);

    glutils::Framebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexturePixelLocalStorageANGLE(0, tex, 0, 0);

    // Clear the error queue. (Should already be empty.)
    while (GLenum err = glGetError())
    {
        fprintf(stderr, "WARNING: unhandled GL error 0x%x\n", err);
    }

    GLint format = GL_NONE;
    glGetFramebufferPixelLocalStorageParameterivANGLE(
        0,
        GL_PIXEL_LOCAL_FORMAT_ANGLE,
        &format);
    return glGetError() == GL_NONE && format == GL_R32UI;
#else
    return false;
#endif
}
} // namespace glutils

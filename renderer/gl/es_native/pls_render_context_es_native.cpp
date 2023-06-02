/*
 * Copyright 2023 Rive
 */

#include "rive/pls/gl/pls_render_context_gl.hpp"

#include "gl/buffer_ring_gl.hpp"
#include "gl/gl_utils.hpp"
#include "rive/math/simd.hpp"
#include "rive/pls/gl/pls_render_target_gl.hpp"
#include <vector>

#ifdef RIVE_ANDROID
#include <EGL/egl.h>

#include "../out/obj/generated/pls_load_store_ext.glsl.hpp"

// We have to manually implement load/store operations from a shader when using
// EXT_shader_pixel_local_storage. These bits define specific operations that can be turned on or
// off in that shader.
namespace loadstoreops
{
constexpr static uint32_t kClearColor = 1;
constexpr static uint32_t kLoadColor = 2;
constexpr static uint32_t kStoreColor = 4;
constexpr static uint32_t kClearCoverage = 8;
constexpr static uint32_t kDeclareClip = 16;
constexpr static uint32_t kClearClip = 32;
}; // namespace loadstoreops
#endif

namespace rive::pls
{
// PLSRenderContext that implements pixel local storage via native OpenGL ES extensions.
class PLSRenderContextESNative : public PLSRenderContextGL
{
public:
    PLSRenderContextESNative();
    ~PLSRenderContextESNative() override;

    rcp<PLSRenderTargetGL> wrapGLRenderTarget(GLuint framebufferID, size_t width, size_t height);
    rcp<PLSRenderTargetGL> makeOffscreenRenderTarget(size_t width, size_t height);
    void activatePixelLocalStorage(LoadAction, const ShaderFeatures&, const DrawProgram&);
    void deactivatePixelLocalStorage(const ShaderFeatures&);

private:
    struct
    {
        bool EXT_shader_framebuffer_fetch = false;
#ifdef RIVE_ANDROID
        bool EXT_shader_pixel_local_storage = false;
        bool ARM_shader_framebuffer_fetch = false;
        bool QCOM_shader_framebuffer_fetch_noncoherent = false;
#endif
    } m_extensions;

#ifdef RIVE_ANDROID
    // On Android we have to load entry points for extensions manually.
    PFNGLFRAMEBUFFERFETCHBARRIERQCOMPROC glFramebufferFetchBarrierQCOM = nullptr;

    // We have to manually implement load/store operations from a shader when using
    // EXT_shader_pixel_local_storage.
    class PLSLoadStoreProgram;
    std::map<uint32_t, PLSLoadStoreProgram> m_plsLoadStorePrograms; // Keyed by loadstoreops.
    GLuint m_plsLoadStoreVertexShader = 0;
    GLuint m_plsLoadStoreVAO = 0;
#endif
};

PLSRenderContextESNative::PLSRenderContextESNative() : PLSRenderContextGL(PlatformFeatures{})
{
    int extensionCount;
    glGetIntegerv(GL_NUM_EXTENSIONS, &extensionCount);
    for (int i = 0; i < extensionCount; ++i)
    {
        auto* ext = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i));
        if (strcmp(ext, "GL_EXT_shader_framebuffer_fetch") == 0)
        {
            m_extensions.EXT_shader_framebuffer_fetch = true;
            continue;
        }
#ifdef RIVE_ANDROID
        if (strcmp(ext, "GL_EXT_shader_pixel_local_storage") == 0)
        {
            m_extensions.EXT_shader_pixel_local_storage = true;
            continue;
        }
        if (strcmp(ext, "GL_ARM_shader_framebuffer_fetch") == 0)
        {
            m_extensions.ARM_shader_framebuffer_fetch = true;
            continue;
        }
        if (strcmp(ext, "GL_QCOM_shader_framebuffer_fetch_noncoherent") == 0)
        {
            glFramebufferFetchBarrierQCOM = reinterpret_cast<PFNGLFRAMEBUFFERFETCHBARRIERQCOMPROC>(
                eglGetProcAddress("glFramebufferFetchBarrierQCOM"));
            m_extensions.QCOM_shader_framebuffer_fetch_noncoherent = glFramebufferFetchBarrierQCOM;
            continue;
        }
#endif
    }
#ifdef RIVE_ANDROID
    if (!m_extensions.EXT_shader_pixel_local_storage && !m_extensions.EXT_shader_framebuffer_fetch)
    {
        printf(
            "EXT_shader_pixel_local_storage and/or EXT_shader_framebuffer_fetch not supported.\n");
        exit(-1);
    }
    if (!m_extensions.ARM_shader_framebuffer_fetch && !m_extensions.EXT_shader_framebuffer_fetch)
    {
        printf("ARM_shader_framebuffer_fetch and/or EXT_shader_framebuffer_fetch not supported.\n");
        exit(-1);
    }
    if (m_extensions.EXT_shader_pixel_local_storage)
    {
        // We have to manually implement load/store operations from a shader when using
        // EXT_shader_pixel_local_storage.
        m_plsLoadStoreVertexShader =
            glutils::CompileShader(GL_VERTEX_SHADER, glsl::pls_load_store_ext);
        glGenVertexArrays(1, &m_plsLoadStoreVAO);
    }
#else
    if (!m_extensions.EXT_shader_framebuffer_fetch)
    {
        printf("EXT_shader_framebuffer_fetch not supported.\n"); // Should never happen on iOS!
        exit(-1);
    }
#endif
}

PLSRenderContextESNative::~PLSRenderContextESNative()
{
#ifdef RIVE_ANDROID
    glDeleteShader(m_plsLoadStoreVertexShader);
    glDeleteVertexArrays(1, &m_plsLoadStoreVAO);
#endif
}

#ifdef RIVE_ANDROID
// Wraps an EXT_shader_pixel_local_storage load/store program, described by a set of loadstoreops.
class PLSRenderContextESNative::PLSLoadStoreProgram
{
public:
    PLSLoadStoreProgram(const PLSLoadStoreProgram&) = delete;
    PLSLoadStoreProgram& operator=(const DrawProgram&) = delete;

    PLSLoadStoreProgram(uint32_t ops, GLuint vertexShader)
    {
        std::vector<const char*> sources;
        if (ops & loadstoreops::kClearColor)
        {
            sources.push_back("#define " GLSL_CLEAR_COLOR "\n");
        }
        if (ops & loadstoreops::kLoadColor)
        {
            sources.push_back("#define " GLSL_LOAD_COLOR "\n");
        }
        if (ops & loadstoreops::kStoreColor)
        {
            sources.push_back("#define " GLSL_STORE_COLOR "\n");
        }
        if (ops & loadstoreops::kClearCoverage)
        {
            sources.push_back("#define " GLSL_CLEAR_COVERAGE "\n");
        }
        if (ops & loadstoreops::kDeclareClip)
        {
            sources.push_back("#define " GLSL_DECLARE_CLIP "\n");
        }
        if (ops & loadstoreops::kClearClip)
        {
            sources.push_back("#define " GLSL_CLEAR_CLIP "\n");
        }
        sources.push_back(glsl::pls_load_store_ext);

        m_id = glCreateProgram();
        glAttachShader(m_id, vertexShader);
        glutils::CompileAndAttachShader(m_id, GL_FRAGMENT_SHADER, sources.data(), sources.size());
        glutils::LinkProgram(m_id);

        if (ops & loadstoreops::kClearColor)
        {
            m_clearColorUniLocation = glGetUniformLocation(m_id, GLSL_clearColor);
        }
    }

    ~PLSLoadStoreProgram() { glDeleteProgram(m_id); }

    void bind(const float clearColor[4]) const
    {
        glUseProgram(m_id);
        if (m_clearColorUniLocation >= 0)
        {
            glUniform4fv(m_clearColorUniLocation, 1, clearColor);
        }
    }

private:
    GLuint m_id;
    GLint m_clearColorUniLocation = -1;
};
#endif

rcp<PLSRenderTargetGL> PLSRenderContextGL::wrapGLRenderTarget(GLuint framebufferID,
                                                              size_t width,
                                                              size_t height)
{
    return static_cast<PLSRenderContextESNative*>(this)->wrapGLRenderTarget(framebufferID,
                                                                            width,
                                                                            height);
}

rcp<PLSRenderTargetGL> PLSRenderContextESNative::wrapGLRenderTarget(GLuint framebufferID,
                                                                    size_t width,
                                                                    size_t height)
{
    PLSRenderTargetGL::CoverageBackingType backingType;
#ifdef RIVE_ANDROID
    if (m_extensions.EXT_shader_pixel_local_storage)
    {
        backingType = PLSRenderTargetGL::CoverageBackingType::memoryless;
    }
    else
#endif
    {
        if (framebufferID == 0)
        {
            return nullptr; // FBO 0 doesn't support additional color attachments.
        }
        backingType = PLSRenderTargetGL::CoverageBackingType::texture;
    }
    return rcp<PLSRenderTargetGL>(
        new PLSRenderTargetGL(framebufferID, width, height, backingType, m_platformFeatures));
}

rcp<PLSRenderTargetGL> PLSRenderContextGL::makeOffscreenRenderTarget(size_t width, size_t height)
{
    return static_cast<PLSRenderContextESNative*>(this)->makeOffscreenRenderTarget(width, height);
}

rcp<PLSRenderTargetGL> PLSRenderContextESNative::makeOffscreenRenderTarget(size_t width,
                                                                           size_t height)
{
    PLSRenderTargetGL::CoverageBackingType backingType;
#ifdef RIVE_ANDROID
    if (m_extensions.EXT_shader_pixel_local_storage)
    {
        backingType = PLSRenderTargetGL::CoverageBackingType::memoryless;
    }
    else
#endif
    {
        backingType = PLSRenderTargetGL::CoverageBackingType::texture;
    }
    return rcp<PLSRenderTargetGL>(
        new PLSRenderTargetGL(width, height, backingType, m_platformFeatures));
}

void PLSRenderContextGL::activatePixelLocalStorage(LoadAction loadAction,
                                                   const ShaderFeatures& shaderFeatures,
                                                   const DrawProgram& drawProgram)
{
    static_cast<PLSRenderContextESNative*>(this)->activatePixelLocalStorage(loadAction,
                                                                            shaderFeatures,
                                                                            drawProgram);
}

constexpr static GLenum kPLSDrawBuffers[4] = {GL_COLOR_ATTACHMENT0,
                                              GL_COLOR_ATTACHMENT1,
                                              GL_COLOR_ATTACHMENT2,
                                              GL_COLOR_ATTACHMENT3};

void PLSRenderContextESNative::activatePixelLocalStorage(LoadAction loadAction,
                                                         const ShaderFeatures& shaderFeatures,
                                                         const DrawProgram& drawProgram)
{
    float clearColor4f[4];
    if (loadAction == LoadAction::clear)
    {
        UnpackColorToRGBA32F(frameDescriptor().clearColor, clearColor4f);
    }
#ifdef RIVE_ANDROID
    if (m_extensions.EXT_shader_pixel_local_storage)
    {
        glEnable(GL_SHADER_PIXEL_LOCAL_STORAGE_EXT);

        uint32_t ops =
            loadAction == LoadAction::clear ? loadstoreops::kClearColor : loadstoreops::kLoadColor;
        ops |= loadstoreops::kClearCoverage;
        if (shaderFeatures.programFeatures.enablePathClipping)
        {
            ops |= loadstoreops::kDeclareClip | loadstoreops::kClearClip;
        }
        if ((ops & loadstoreops::kClearColor) && simd::all(simd::load4f(clearColor4f) == 0))
        {
            // glClear() is only well-defined for EXT_shader_pixel_local_storage when the clear
            // color is zero, and it zeros out every plane of pixel local storage.
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        else
        {
            // Otherwise we have to initialize pixel local storage with a fullscreen draw.
            const PLSLoadStoreProgram& plsProgram =
                m_plsLoadStorePrograms.try_emplace(ops, ops, m_plsLoadStoreVertexShader)
                    .first->second;
            plsProgram.bind(clearColor4f);
            glBindVertexArray(m_plsLoadStoreVAO);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    }
    else
#endif
    {
        assert(m_extensions.EXT_shader_framebuffer_fetch);

        // Enable multiple render targets, with a draw buffer for each PLS plane.
        GLsizei numDrawBuffers = shaderFeatures.programFeatures.enablePathClipping ? 4 : 3;
        glDrawBuffers(numDrawBuffers, kPLSDrawBuffers);

        // Instruct the driver not to load existing PLS plane contents into tiled memory, with the
        // exception of the color buffer after an intermediate flush.
        static_assert(kFramebufferPlaneIdx == 0);
        glInvalidateFramebuffer(
            GL_FRAMEBUFFER,
            loadAction == LoadAction::clear ? numDrawBuffers : numDrawBuffers - 1,
            loadAction == LoadAction::clear ? kPLSDrawBuffers : kPLSDrawBuffers + 1);

        // Clear the PLS planes.
        constexpr static uint32_t kZero[4]{};
        if (loadAction == LoadAction::clear)
        {
            glClearBufferfv(GL_COLOR, kFramebufferPlaneIdx, clearColor4f);
        }
        glClearBufferuiv(GL_COLOR, kCoveragePlaneIdx, kZero);
        if (shaderFeatures.programFeatures.enablePathClipping)
        {
            glClearBufferuiv(GL_COLOR, kClipPlaneIdx, kZero);
        }
    }

    glBindVertexArray(m_drawVAO);
    static_cast<const UniformBufferGL*>(drawUniformBufferRing())
        ->bindToUniformBlock(kUniformBlockIdx);
    drawProgram.bind();
}

void PLSRenderContextGL::deactivatePixelLocalStorage(const ShaderFeatures& shaderFeatures)
{
    static_cast<PLSRenderContextESNative*>(this)->deactivatePixelLocalStorage(shaderFeatures);
}

void PLSRenderContextESNative::deactivatePixelLocalStorage(const ShaderFeatures& shaderFeatures)
{
#ifdef RIVE_ANDROID
    if (m_extensions.EXT_shader_pixel_local_storage)
    {
        // Issue a fullscreen draw that transfers the color information in pixel local storage to
        // the main framebuffer.
        uint32_t ops = loadstoreops::kStoreColor;
        if (shaderFeatures.programFeatures.enablePathClipping)
        {
            ops |= loadstoreops::kDeclareClip;
        }
        const PLSLoadStoreProgram& plsProgram =
            m_plsLoadStorePrograms.try_emplace(ops, ops, m_plsLoadStoreVertexShader).first->second;
        plsProgram.bind(nullptr);
        glBindVertexArray(m_plsLoadStoreVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glDisable(GL_SHADER_PIXEL_LOCAL_STORAGE_EXT);
    }
    else
#endif
    {
        assert(m_extensions.EXT_shader_framebuffer_fetch);

        // Instruct the driver not to flush PLS contents from tiled memory, with the exception of
        // the color buffer.
        static_assert(kFramebufferPlaneIdx == 0);
        GLsizei numDrawBuffers = shaderFeatures.programFeatures.enablePathClipping ? 4 : 3;
        glInvalidateFramebuffer(GL_FRAMEBUFFER, numDrawBuffers - 1, kPLSDrawBuffers + 1);

        // In EXT_shader_framebuffer_fetch mode we render to an internal framebuffer, so there's no
        // need to restore glDrawBuffers.
    }
}

std::unique_ptr<PLSRenderContextGL> PLSRenderContextGL::Make()
{
    return std::make_unique<PLSRenderContextESNative>();
}
} // namespace rive::pls

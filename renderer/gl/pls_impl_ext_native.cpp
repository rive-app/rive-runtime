/*
 * Copyright 2023 Rive
 */

#include "rive/pls/gl/pls_render_context_gl_impl.hpp"

#include "gl/gl_utils.hpp"
#include "rive/math/simd.hpp"
#include "rive/pls/gl/pls_render_target_gl.hpp"
#include <vector>

#include "../out/obj/generated/glsl.exports.h"
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
constexpr static uint32_t kClearClip = 16;
}; // namespace loadstoreops

namespace rive::pls
{
// Wraps an EXT_shader_pixel_local_storage load/store program, described by a set of loadstoreops.
class PLSLoadStoreProgram
{
public:
    PLSLoadStoreProgram(const PLSLoadStoreProgram&) = delete;
    PLSLoadStoreProgram& operator=(const PLSLoadStoreProgram&) = delete;

    PLSLoadStoreProgram(uint32_t ops, GLuint vertexShader, const GLExtensions& extensions)
    {
        std::vector<const char*> defines{GLSL_PLS_IMPL_EXT_NATIVE};
        if (ops & loadstoreops::kClearColor)
        {
            defines.push_back(GLSL_CLEAR_COLOR);
        }
        if (ops & loadstoreops::kLoadColor)
        {
            defines.push_back(GLSL_LOAD_COLOR);
        }
        if (ops & loadstoreops::kStoreColor)
        {
            defines.push_back(GLSL_STORE_COLOR);
        }
        if (ops & loadstoreops::kClearCoverage)
        {
            defines.push_back(GLSL_CLEAR_COVERAGE);
        }
        if (ops & loadstoreops::kClearClip)
        {
            defines.push_back(GLSL_CLEAR_CLIP);
        }

        const char* source = glsl::pls_load_store_ext;

        m_id = glCreateProgram();
        glAttachShader(m_id, vertexShader);
        glutils::CompileAndAttachShader(m_id,
                                        GL_FRAGMENT_SHADER,
                                        defines.data(),
                                        defines.size(),
                                        &source,
                                        1,
                                        extensions);
        glutils::LinkProgram(m_id);

        if (ops & loadstoreops::kClearColor)
        {
            m_clearColorUniLocation = glGetUniformLocation(m_id, GLSL_clearColor);
        }
    }

    ~PLSLoadStoreProgram() { glDeleteProgram(m_id); }

    GLuint id() const { return m_id; }
    GLint clearColorUniLocation() const { return m_clearColorUniLocation; }

private:
    GLuint m_id;
    GLint m_clearColorUniLocation = -1;
};

class PLSRenderContextGLImpl::PLSImplEXTNative : public PLSRenderContextGLImpl::PLSImpl
{
public:
    PLSImplEXTNative(const GLExtensions& extensions) : m_extensions(extensions)
    {
        // We have to manually implement load/store operations from a shader when using
        // EXT_shader_pixel_local_storage.
        m_plsLoadStoreVertexShader =
            glutils::CompileShader(GL_VERTEX_SHADER, glsl::pls_load_store_ext, extensions);
        glGenVertexArrays(1, &m_plsLoadStoreVAO);
    }

    ~PLSImplEXTNative()
    {
        glDeleteShader(m_plsLoadStoreVertexShader);
        glDeleteVertexArrays(1, &m_plsLoadStoreVAO);
    }

    rcp<PLSRenderTargetGL> wrapGLRenderTarget(GLuint framebufferID,
                                              size_t width,
                                              size_t height,
                                              const PlatformFeatures& platformFeatures) override
    {
        return rcp(new PLSRenderTargetGL(framebufferID, width, height, platformFeatures));
    }

    rcp<PLSRenderTargetGL> makeOffscreenRenderTarget(
        size_t width,
        size_t height,
        const PlatformFeatures& platformFeatures) override
    {
        return rcp(new PLSRenderTargetGL(width, height, platformFeatures));
    }

    void activatePixelLocalStorage(PLSRenderContextGLImpl* impl,
                                   const PLSRenderContext::FlushDescriptor& desc) override
    {
        assert(impl->m_extensions.EXT_shader_pixel_local_storage);
        assert(impl->m_extensions.EXT_shader_framebuffer_fetch ||
               impl->m_extensions.ARM_shader_framebuffer_fetch);

        auto renderTarget = static_cast<const PLSRenderTargetGL*>(desc.renderTarget);
        glBindFramebuffer(GL_FRAMEBUFFER, renderTarget->drawFramebufferID());
        glEnable(GL_SHADER_PIXEL_LOCAL_STORAGE_EXT);

        uint32_t ops = loadstoreops::kClearCoverage;
        float clearColor4f[4];
        if (desc.loadAction == LoadAction::clear)
        {
            UnpackColorToRGBA32F(desc.clearColor, clearColor4f);
            ops |= loadstoreops::kClearColor;
        }
        else
        {
            ops |= loadstoreops::kLoadColor;
        }
        if (desc.needsClipBuffer)
        {
            ops |= loadstoreops::kClearClip;
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
                m_plsLoadStorePrograms
                    .try_emplace(ops, ops, m_plsLoadStoreVertexShader, m_extensions)
                    .first->second;
            impl->bindProgram(plsProgram.id());
            if (plsProgram.clearColorUniLocation() >= 0)
            {
                glUniform4fv(plsProgram.clearColorUniLocation(), 1, clearColor4f);
            }
            impl->bindVAO(m_plsLoadStoreVAO);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    }

    void deactivatePixelLocalStorage(PLSRenderContextGLImpl* impl) override
    {
        // Issue a fullscreen draw that transfers the color information in pixel local storage to
        // the main framebuffer.
        uint32_t ops = loadstoreops::kStoreColor;
        const PLSLoadStoreProgram& plsProgram =
            m_plsLoadStorePrograms.try_emplace(ops, ops, m_plsLoadStoreVertexShader, m_extensions)
                .first->second;
        impl->bindProgram(plsProgram.id());
        impl->bindVAO(m_plsLoadStoreVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glDisable(GL_SHADER_PIXEL_LOCAL_STORAGE_EXT);
    }

    const char* shaderDefineName() const override { return GLSL_PLS_IMPL_EXT_NATIVE; }

private:
    const GLExtensions m_extensions;
    std::map<uint32_t, PLSLoadStoreProgram> m_plsLoadStorePrograms; // Keyed by loadstoreops.
    GLuint m_plsLoadStoreVertexShader = 0;
    GLuint m_plsLoadStoreVAO = 0;
};

std::unique_ptr<PLSRenderContextGLImpl::PLSImpl> PLSRenderContextGLImpl::MakePLSImplEXTNative(
    const GLExtensions& extensions)
{
    return std::make_unique<PLSImplEXTNative>(extensions);
}
} // namespace rive::pls

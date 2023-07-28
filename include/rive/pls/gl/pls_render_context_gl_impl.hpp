/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/math/mat2d.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/pls/gl/gles3.hpp"
#include "rive/pls/gl/pls_render_target_gl.hpp"
#include "rive/pls/buffer_ring.hpp"
#include "rive/pls/pls_render_context_buffer_ring_impl.hpp"
#include <map>

namespace rive::pls
{
class PLSPath;
class PLSPaint;
class PLSRenderTargetGL;

// OpenGL backend implementation of PLSRenderContext.
class PLSRenderContextGLImpl : public PLSRenderContextBufferRingImpl
{
public:
    static std::unique_ptr<PLSRenderContextGLImpl> Make();
    ~PLSRenderContextGLImpl() override;

    // Creates a PLSRenderTarget that draws directly into the given GL framebuffer.
    // Returns null if the framebuffer doesn't support pixel local storage.
    rcp<PLSRenderTargetGL> wrapGLRenderTarget(GLuint framebufferID, size_t width, size_t height)
    {
        return m_plsImpl->wrapGLRenderTarget(framebufferID, width, height, platformFeatures());
    }

    // Creates a PLSRenderTarget that draws to a new, offscreen GL framebuffer. This method is
    // guaranteed to succeed, but the caller must call glBlitFramebuffer() to view the rendering
    // results.
    rcp<PLSRenderTargetGL> makeOffscreenRenderTarget(size_t width, size_t height)
    {
        return m_plsImpl->makeOffscreenRenderTarget(width, height, platformFeatures());
    }

private:
    class DrawProgram;

    // Manages how we implement pixel local storage in shaders.
    class PLSImpl
    {
    public:
        virtual rcp<PLSRenderTargetGL> wrapGLRenderTarget(GLuint framebufferID,
                                                          size_t width,
                                                          size_t height,
                                                          const PlatformFeatures&) = 0;
        virtual rcp<PLSRenderTargetGL> makeOffscreenRenderTarget(size_t width,
                                                                 size_t height,
                                                                 const PlatformFeatures&) = 0;

        virtual void activatePixelLocalStorage(PLSRenderContextGLImpl*,
                                               const PLSRenderContext::FlushDescriptor&) = 0;
        virtual void deactivatePixelLocalStorage(PLSRenderContextGLImpl*) = 0;

        virtual const char* shaderDefineName() const = 0;

        void ensureRasterOrderingEnabled(bool enabled)
        {
            if (m_rasterOrderingEnabled != enabled)
            {
                onBarrier();
                onEnableRasterOrdering(enabled);
                m_rasterOrderingEnabled = enabled;
            }
        }

        virtual void barrier()
        {
            assert(!m_rasterOrderingEnabled);
            onBarrier();
        }

        virtual ~PLSImpl() {}

    private:
        virtual void onEnableRasterOrdering(bool enabled) {}
        virtual void onBarrier() {}

        bool m_rasterOrderingEnabled = true;
    };

    class PLSImplEXTNative;
    class PLSImplFramebufferFetch;
    class PLSImplWebGL;
    class PLSImplRWTexture;

    static std::unique_ptr<PLSImpl> MakePLSImplEXTNative(const GLExtensions&);
    static std::unique_ptr<PLSImpl> MakePLSImplFramebufferFetch(const GLExtensions&);
    static std::unique_ptr<PLSImpl> MakePLSImplWebGL();
    static std::unique_ptr<PLSImpl> MakePLSImplRWTexture();

    // Wraps a compiled and linked GL program of draw.glsl, with a specific set of features enabled
    // via #define. The set of features to enable is dictated by ShaderFeatures.
    class DrawProgram
    {
    public:
        DrawProgram(const DrawProgram&) = delete;
        DrawProgram& operator=(const DrawProgram&) = delete;
        DrawProgram(PLSRenderContextGLImpl*, DrawType, const ShaderFeatures&);
        ~DrawProgram();

        GLuint id() const { return m_id; }
        GLint baseInstancePolyfillLocation() const { return m_baseInstancePolyfillLocation; }

    private:
        GLuint m_id;
        GLint m_baseInstancePolyfillLocation = -1;
    };

    class DrawShader;

    PLSRenderContextGLImpl(const PlatformFeatures&, GLExtensions, std::unique_ptr<PLSImpl>);

    std::unique_ptr<BufferRingImpl> makeVertexBufferRing(size_t capacity,
                                                         size_t itemSizeInBytes) override;

    std::unique_ptr<TexelBufferRing> makeTexelBufferRing(TexelBufferRing::Format,
                                                         size_t widthInItems,
                                                         size_t height,
                                                         size_t texelsPerItem,
                                                         int textureIdx,
                                                         TexelBufferRing::Filter) override;

    std::unique_ptr<BufferRingImpl> makePixelUnpackBufferRing(size_t capacity,
                                                              size_t itemSizeInBytes) override;

    std::unique_ptr<BufferRingImpl> makeUniformBufferRing(size_t sizeInBytes) override;

    void allocateGradientTexture(size_t height) override;
    void allocateTessellationTexture(size_t height) override;

    void flush(const PLSRenderContext::FlushDescriptor&) override;

    // GL state wrapping.
    void bindProgram(GLuint);
    void bindVAO(GLuint);

    GLExtensions m_extensions;

    constexpr static size_t kShaderVersionStringBuffSize = sizeof("#version 300 es\n") + 1;
    char m_shaderVersionString[kShaderVersionStringBuffSize];

    std::unique_ptr<PLSImpl> m_plsImpl;

    // Gradient texture rendering.
    GLuint m_colorRampProgram;
    GLuint m_colorRampVAO;
    GLuint m_colorRampFBO;
    GLuint m_gradientTexture = 0;

    // Tessellation texture rendering.
    GLuint m_tessellateProgram;
    GLuint m_tessellateVAO;
    GLuint m_tessellateFBO;
    GLuint m_tessVertexTexture = 0;

    // Not all programs have a unique vertex shader, so we cache and reuse them where possible.
    std::map<uint32_t, DrawShader> m_vertexShaders;
    std::map<uint32_t, DrawProgram> m_drawPrograms;
    GLuint m_drawVAO;
    GLuint m_interiorTrianglesVAO;
    GLuint m_patchVerticesBuffer;
    GLuint m_patchIndicesBuffer;

    // Cached GL state.
    GLuint m_boundProgramID = 0;
    GLuint m_boundVAO = 0;
};
} // namespace rive::pls

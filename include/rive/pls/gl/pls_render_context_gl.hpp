/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/math/mat2d.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/pls/gl/gles3.hpp"
#include "rive/pls/gl/pls_render_target_gl.hpp"
#include "rive/pls/buffer_ring.hpp"
#include "rive/pls/pls_render_context.hpp"
#include <map>

namespace rive::pls
{
class PLSPath;
class PLSPaint;
class PLSRenderTargetGL;

// OpenGL backend implementation of PLSRenderContext.
class PLSRenderContextGL : public PLSRenderContext
{
public:
    static std::unique_ptr<PLSRenderContextGL> Make();
    ~PLSRenderContextGL() override;

    // Creates a PLSRenderTarget that draws directly into the given GL framebuffer.
    // Returns null if the framebuffer doesn't support pixel local storage.
    rcp<PLSRenderTargetGL> wrapGLRenderTarget(GLuint framebufferID, size_t width, size_t height)
    {
        return m_plsImpl->wrapGLRenderTarget(framebufferID, width, height, m_platformFeatures);
    }

    // Creates a PLSRenderTarget that draws to a new, offscreen GL framebuffer. This method is
    // guaranteed to succeed, but the caller must call glBlitFramebuffer() to view the rendering
    // results.
    rcp<PLSRenderTargetGL> makeOffscreenRenderTarget(size_t width, size_t height)
    {
        return m_plsImpl->makeOffscreenRenderTarget(width, height, m_platformFeatures);
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

        virtual void activatePixelLocalStorage(PLSRenderContextGL*,
                                               const PLSRenderTargetGL*,
                                               LoadAction,
                                               bool needsClipBuffer) = 0;
        virtual void deactivatePixelLocalStorage() = 0;

        virtual const char* shaderDefineName() const = 0;

        virtual ~PLSImpl() {}
    };

    class PLSImplEXTNative;
    class PLSImplFramebufferFetch;
    class PLSImplWebGL;
    class PLSImplRWTexture;

    static std::unique_ptr<PLSImpl> MakePLSImplEXTNative();
    static std::unique_ptr<PLSImpl> MakePLSImplFramebufferFetch();
    static std::unique_ptr<PLSImpl> MakePLSImplWebGL();
    static std::unique_ptr<PLSImpl> MakePLSImplRWTexture();

    // Wraps a compiled and linked GL program of draw.glsl, with a specific set of features enabled
    // via #define. The set of features to enable is dictated by ShaderFeatures.
    class DrawProgram
    {
    public:
        DrawProgram(const DrawProgram&) = delete;
        DrawProgram& operator=(const DrawProgram&) = delete;
        DrawProgram(PLSRenderContextGL* context, const ShaderFeatures& shaderFeatures);
        ~DrawProgram();

        GLuint id() const { return m_id; }
        GLint baseInstancePolyfillLocation() const { return m_baseInstancePolyfillLocation; }

    private:
        GLuint m_id;
        GLint m_baseInstancePolyfillLocation = -1;
    };

    class DrawShader;

    PLSRenderContextGL(const PlatformFeatures&,
                       const GLExtensions& extensions,
                       std::unique_ptr<PLSImpl>);

    const PLSRenderTargetGL* renderTarget() const
    {
        return static_cast<const PLSRenderTargetGL*>(frameDescriptor().renderTarget.get());
    }

    std::unique_ptr<BufferRingImpl> makeVertexBufferRing(size_t capacity,
                                                         size_t itemSizeInBytes) override;

    std::unique_ptr<TexelBufferRing> makeTexelBufferRing(TexelBufferRing::Format,
                                                         size_t widthInItems,
                                                         size_t height,
                                                         size_t texelsPerItem,
                                                         int textureIdx,
                                                         TexelBufferRing::Filter) override;

    std::unique_ptr<BufferRingImpl> makeUniformBufferRing(size_t capacity,
                                                          size_t sizeInBytes) override;

    void allocateTessellationTexture(size_t height) override;

    void onFlush(FlushType,
                 LoadAction,
                 size_t gradSpanCount,
                 size_t gradSpansHeight,
                 size_t tessVertexSpanCount,
                 size_t tessDataHeight,
                 bool needsClipBuffer) override;

    GLExtensions m_extensions;

    constexpr static size_t kShaderVersionStringBuffSize = sizeof("#version 300 es\n") + 1;
    char m_shaderVersionString[kShaderVersionStringBuffSize];
    bool m_supportsBaseInstanceInShader = false;

    std::unique_ptr<PLSImpl> m_plsImpl;

    // Gradient texture rendering.
    GLuint m_colorRampProgram;
    GLuint m_colorRampVAO;
    GLuint m_colorRampFBO;

    // Tessellation texture rendering.
    GLuint m_tessellateProgram;
    GLuint m_tessellateVAO;
    GLuint m_tessellateFBO;
    GLuint m_tessVertexTexture = 0;

    // Not all programs have a unique vertex shader, so we cache and reuse them where possible.
    std::map<uint64_t, DrawShader> m_vertexShaders;
    std::map<uint64_t, DrawProgram> m_drawPrograms;
    GLuint m_drawVAO;
    GLuint m_pathWedgeVertexBuffer;
    GLuint m_pathWedgeIndexBuffer;
};
} // namespace rive::pls

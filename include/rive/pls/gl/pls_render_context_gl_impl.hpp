/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/pls/gl/gl_state.hpp"
#include "rive/pls/pls_render_context_helper_impl.hpp"
#include <map>

namespace rive::pls
{
class PLSPath;
class PLSPaint;
class PLSRenderTargetGL;

// OpenGL backend implementation of PLSRenderContextImpl.
class PLSRenderContextGLImpl : public PLSRenderContextHelperImpl
{
public:
    struct ContextOptions
    {
        bool disableFragmentShaderInterlock = false;
    };

    static std::unique_ptr<PLSRenderContext> MakeContext(const ContextOptions&);
    static std::unique_ptr<PLSRenderContext> MakeContext() { return MakeContext(ContextOptions()); }

    ~PLSRenderContextGLImpl() override;

    const GLCapabilities& capabilities() const { return m_capabilities; }

    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType, RenderBufferFlags, size_t) override;

    rcp<PLSTexture> makeImageTexture(uint32_t width,
                                     uint32_t height,
                                     uint32_t mipLevelCount,
                                     const uint8_t imageDataRGBA[]) override;

    // Called *after* the GL context has been modified externally.
    // Re-binds Rive internal resources and invalidates the internal cache of GL state.
    void invalidateGLState();

    // Called *before* the GL context will be modified externally.
    // Unbinds Rive internal resources before yielding control of the GL context.
    void unbindGLInternalResources();

    // Utility for rendering a texture to an MSAA framebuffer, since glBlitFramebuffer() doesn't
    // support copying non-MSAA to MSAA.
    void blitTextureToFramebufferAsDraw(GLuint textureID,
                                        const IAABB& bounds,
                                        uint32_t renderTargetHeight);

    GLState* state() const { return m_state.get(); }

private:
    class DrawProgram;

    // Manages how we implement pixel local storage in shaders.
    class PLSImpl
    {
    public:
        virtual void init(rcp<GLState>) {}

        virtual bool supportsRasterOrdering(const GLCapabilities&) const = 0;

        virtual void activatePixelLocalStorage(PLSRenderContextGLImpl*, const FlushDescriptor&) = 0;
        virtual void deactivatePixelLocalStorage(PLSRenderContextGLImpl*,
                                                 const FlushDescriptor&) = 0;

        // Depending on how we handle PLS atomic resolves, the PLSImpl may require certain flags.
        virtual pls::ShaderMiscFlags atomicResolveShaderMiscFlags(const pls::FlushDescriptor&) const
        {
            return pls::ShaderMiscFlags::none;
        }

        // Called before issuing a plsAtomicResolve draw, so the PLSImpl can make any necessary GL
        // state changes.
        virtual void setupAtomicResolve(PLSRenderContextGLImpl*, const pls::FlushDescriptor&) {}

        virtual const char* shaderDefineName() const = 0;

        void ensureRasterOrderingEnabled(PLSRenderContextGLImpl* plsContextImpl, bool enabled);

        void barrier()
        {
            assert(m_rasterOrderingEnabled == pls::TriState::no);
            onBarrier();
        }

        virtual ~PLSImpl() {}

    private:
        virtual void onEnableRasterOrdering(bool enabled) {}
        virtual void onBarrier() {}

        pls::TriState m_rasterOrderingEnabled = pls::TriState::unknown;
    };

    class PLSImplEXTNative;
    class PLSImplFramebufferFetch;
    class PLSImplWebGL;
    class PLSImplRWTexture;

    static std::unique_ptr<PLSImpl> MakePLSImplEXTNative(const GLCapabilities&);
    static std::unique_ptr<PLSImpl> MakePLSImplFramebufferFetch(const GLCapabilities&);
    static std::unique_ptr<PLSImpl> MakePLSImplWebGL();
    static std::unique_ptr<PLSImpl> MakePLSImplRWTexture();

    static std::unique_ptr<PLSRenderContext> MakeContext(const char* rendererString,
                                                         GLCapabilities,
                                                         std::unique_ptr<PLSImpl>);

    PLSRenderContextGLImpl(const char* rendererString, GLCapabilities, std::unique_ptr<PLSImpl>);

    // Wraps a compiled and linked GL program of draw_path.glsl or draw_image_mesh.glsl, with a
    // specific set of features enabled via #define. The set of features to enable is dictated by
    // ShaderFeatures.
    class DrawProgram
    {
    public:
        DrawProgram(const DrawProgram&) = delete;
        DrawProgram& operator=(const DrawProgram&) = delete;
        DrawProgram(PLSRenderContextGLImpl*,
                    pls::DrawType,
                    pls::ShaderFeatures,
                    pls::InterlockMode,
                    pls::ShaderMiscFlags);
        ~DrawProgram();

        GLuint id() const { return m_id; }
        GLint spirvCrossBaseInstanceLocation() const { return m_spirvCrossBaseInstanceLocation; }

    private:
        GLuint m_id;
        GLint m_spirvCrossBaseInstanceLocation = -1;
        const rcp<GLState> m_state;
    };

    class DrawShader;

    std::unique_ptr<BufferRing> makeUniformBufferRing(size_t capacityInBytes) override;
    std::unique_ptr<BufferRing> makeStorageBufferRing(size_t capacityInBytes,
                                                      pls::StorageBufferStructure) override;
    std::unique_ptr<BufferRing> makeVertexBufferRing(size_t capacityInBytes) override;
    std::unique_ptr<BufferRing> makeTextureTransferBufferRing(size_t capacityInBytes) override;

    void resizeGradientTexture(uint32_t width, uint32_t height) override;
    void resizeTessellationTexture(uint32_t width, uint32_t height) override;

    void flush(const FlushDescriptor&) override;

    GLCapabilities m_capabilities;

    std::unique_ptr<PLSImpl> m_plsImpl;

    // Gradient texture rendering.
    GLuint m_colorRampProgram;
    GLuint m_colorRampVAO;
    GLuint m_colorRampFBO;
    GLuint m_gradientTexture = 0;

    // Tessellation texture rendering.
    GLuint m_tessellateProgram;
    GLuint m_tessellateVAO;
    GLuint m_tessSpanIndexBuffer;
    GLuint m_tessellateFBO;
    GLuint m_tessVertexTexture = 0;

    // Not all programs have a unique vertex shader, so we cache and reuse them where possible.
    std::map<uint32_t, DrawShader> m_vertexShaders;
    std::map<uint32_t, DrawProgram> m_drawPrograms;

    // Vertex/index buffers for drawing paths.
    GLuint m_drawVAO = 0;
    GLuint m_patchVerticesBuffer = 0;
    GLuint m_patchIndicesBuffer = 0;
    GLuint m_trianglesVAO = 0;

    // Vertex/index buffers for drawing image rects. (Atomic mode only, and only used when bindless
    // textures aren't supported.)
    GLuint m_imageRectVAO = 0;
    GLuint m_imageRectVertexBuffer = 0;
    GLuint m_imageRectIndexBuffer = 0;

    GLuint m_imageMeshVAO = 0;
    GLuint m_emptyVAO = 0;

    // Used for blitting non-MSAA -> MSAA, which isn't supported by glBlitFramebuffer().
    GLuint m_blitAsDrawProgram = 0;

    const rcp<GLState> m_state;
};
} // namespace rive::pls

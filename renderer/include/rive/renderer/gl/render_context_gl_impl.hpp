/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/renderer/async_pipeline_manager.hpp"
#include "rive/renderer/gl/gl_state.hpp"
#include "rive/renderer/gl/gl_utils.hpp"
#include "rive/renderer/render_context_helper_impl.hpp"
#include "rive/renderer/vertex_shader_manager.hpp"

namespace rive
{
class RiveRenderPath;
class RiveRenderPaint;
} // namespace rive

namespace rive::gpu
{
class RenderTargetGL;

// OpenGL backend implementation of RenderContextImpl.
class RenderContextGLImpl : public RenderContextHelperImpl
{
public:
    struct ContextOptions
    {
        ShaderCompilationMode shaderCompilationMode =
            ShaderCompilationMode::standard;
        bool disablePixelLocalStorage = false;
        bool disableFragmentShaderInterlock = false;
    };

    static std::unique_ptr<RenderContext> MakeContext(const ContextOptions&);
    static std::unique_ptr<RenderContext> MakeContext()
    {
        return MakeContext(ContextOptions());
    }

    ~RenderContextGLImpl() override;

    const GLCapabilities& capabilities() const { return m_capabilities; }

    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType,
                                       RenderBufferFlags,
                                       size_t) override;

    rcp<Texture> makeImageTexture(uint32_t width,
                                  uint32_t height,
                                  uint32_t mipLevelCount,
                                  const uint8_t imageDataRGBAPremul[]) override;

    // Takes ownership of textureID and responsibility for deleting it.
    rcp<Texture> adoptImageTexture(uint32_t width,
                                   uint32_t height,
                                   GLuint textureID);

    // Called *after* the GL context has been modified externally.
    // Re-binds Rive internal resources and invalidates the internal cache of GL
    // state.
    void invalidateGLState();

    // Called *before* the GL context will be modified externally.
    // Unbinds Rive internal resources before yielding control of the GL
    // context.
    void unbindGLInternalResources();

    // Utility for rendering a texture to an MSAA framebuffer, since
    // glBlitFramebuffer() doesn't support copying non-MSAA to MSAA.
    void blitTextureToFramebufferAsDraw(GLuint textureID,
                                        const IAABB& bounds,
                                        uint32_t renderTargetHeight);

    GLState* state() const { return m_state.get(); }

private:
    class DrawProgram;

    // Manages how we implement pixel local storage in shaders.
    class PixelLocalStorageImpl
    {
    public:
        virtual void init(rcp<GLState>) {}

        virtual bool supportsRasterOrdering(const GLCapabilities&) const = 0;
        virtual bool supportsFragmentShaderAtomics(
            const GLCapabilities&) const = 0;

        virtual void activatePixelLocalStorage(RenderContextGLImpl*,
                                               const FlushDescriptor&) = 0;
        virtual void deactivatePixelLocalStorage(RenderContextGLImpl*,
                                                 const FlushDescriptor&) = 0;

        // Depending on how we handle PLS atomic resolves, the
        // PixelLocalStorageImpl may require certain flags.
        virtual gpu::ShaderMiscFlags shaderMiscFlags(
            const gpu::FlushDescriptor&,
            gpu::DrawType) const
        {
            return gpu::ShaderMiscFlags::none;
        }

        virtual void pushShaderDefines(
            gpu::InterlockMode,
            std::vector<const char*>* defines) const = 0;

        void ensureRasterOrderingEnabled(RenderContextGLImpl*,
                                         const gpu::FlushDescriptor&,
                                         bool enabled);

        void barrier(const gpu::FlushDescriptor& desc)
        {
            assert(m_rasterOrderingEnabled == gpu::TriState::no);
            onBarrier(desc);
        }

        virtual ~PixelLocalStorageImpl() {}

#ifndef NDEBUG
        bool rasterOrderingKnownDisabled() const
        {
            return m_rasterOrderingEnabled == gpu::TriState::no;
        }
#endif

    private:
        virtual void onEnableRasterOrdering(bool enabled) {}
        virtual void onBarrier(const gpu::FlushDescriptor& desc) {}

        gpu::TriState m_rasterOrderingEnabled = gpu::TriState::unknown;
    };

    class PLSImplEXTNative;
    class PLSImplFramebufferFetch;
    class PLSImplWebGL;
    class PLSImplRWTexture;

    static std::unique_ptr<PixelLocalStorageImpl> MakePLSImplEXTNative(
        const GLCapabilities&);
    static std::unique_ptr<PixelLocalStorageImpl> MakePLSImplWebGL();
    static std::unique_ptr<PixelLocalStorageImpl> MakePLSImplRWTexture();

    static std::unique_ptr<RenderContext> MakeContext(
        const char* rendererString,
        GLCapabilities,
        std::unique_ptr<PixelLocalStorageImpl>,
        ShaderCompilationMode);

    RenderContextGLImpl(const char* rendererString,
                        GLCapabilities,
                        std::unique_ptr<PixelLocalStorageImpl>,
                        ShaderCompilationMode);

    std::unique_ptr<BufferRing> makeUniformBufferRing(
        size_t capacityInBytes) override;
    std::unique_ptr<BufferRing> makeStorageBufferRing(
        size_t capacityInBytes,
        gpu::StorageBufferStructure) override;
    std::unique_ptr<BufferRing> makeVertexBufferRing(
        size_t capacityInBytes) override;

    void resizeGradientTexture(uint32_t width, uint32_t height) override;
    void resizeTessellationTexture(uint32_t width, uint32_t height) override;
    void resizeAtlasTexture(uint32_t width, uint32_t height) override;

    void flush(const FlushDescriptor&) override;

    // Issues the equivalent of glDrawElementsInstancedBaseInstanceEXT(),
    // assuming no vertex attribs are instanced, indices are uint16_t, and
    // applying workarounds for known driver bugs.
    //
    // If ANGLE_base_vertex_base_instance_shader_builtin is not supported,
    // the intended value of gl_BaseInstance is supplied via
    // baseInstanceUniformLocation.
    void drawIndexedInstancedNoInstancedAttribs(
        GLenum primitiveTopology,
        uint32_t indexCount,
        uint32_t baseIndex,
        uint32_t instanceCount,
        uint32_t baseInstance,
        GLint baseInstanceUniformLocation);

    GLCapabilities m_capabilities;

    std::unique_ptr<PixelLocalStorageImpl> m_plsImpl;

    // Gradient texture rendering.
    glutils::Program m_colorRampProgram;
    glutils::VAO m_colorRampVAO;
    glutils::Framebuffer m_colorRampFBO;
    GLuint m_gradientTexture = 0;

    // Gaussian integral table for feathering.
    glutils::Texture m_featherTexture;

    // Tessellation texture rendering.
    glutils::Program m_tessellateProgram;
    glutils::VAO m_tessellateVAO;
    glutils::Buffer m_tessSpanIndexBuffer;
    glutils::Framebuffer m_tessellateFBO;
    GLuint m_tessVertexTexture = 0;

    // Atlas rendering.
    class AtlasProgram
    {
    public:
        void compile(GLuint vertexShaderID,
                     const char* defines[],
                     size_t numDefines,
                     const char* sources[],
                     size_t numSources,
                     const GLCapabilities&,
                     GLState* state);

        operator GLuint() const { return m_program; }

        GLint baseInstanceUniformLocation() const
        {
            return m_baseInstanceUniformLocation;
        }

    private:
        glutils::Program m_program = glutils::Program::Zero();
        GLint m_baseInstanceUniformLocation = -1;
    };

    glutils::Shader m_atlasVertexShader;
    AtlasProgram m_atlasFillProgram;
    AtlasProgram m_atlasStrokeProgram;
    glutils::Texture m_atlasTexture = glutils::Texture::Zero();
    glutils::Framebuffer m_atlasFBO;

    // Wraps a compiled GL shader of draw_path.glsl or draw_image_mesh.glsl,
    // either vertex or fragment, with a specific set of features enabled via
    // #define. The set of features to enable is dictated by ShaderFeatures.
    class DrawShader
    {
    public:
        DrawShader() = default;
        DrawShader(const DrawShader&) = delete;
        DrawShader& operator=(const DrawShader&) = delete;
        DrawShader(DrawShader&&);
        DrawShader& operator=(DrawShader&&);

        DrawShader(RenderContextGLImpl* renderContextImpl,
                   GLenum shaderType,
                   gpu::DrawType drawType,
                   ShaderFeatures shaderFeatures,
                   gpu::InterlockMode interlockMode,
                   gpu::ShaderMiscFlags shaderMiscFlags);

        ~DrawShader();

        GLuint id() const { return m_id; }

    private:
        GLuint m_id = 0;
    };

    // Wraps a compiled and linked GL program of draw_path.glsl or
    // draw_image_mesh.glsl, with a specific set of features enabled via
    // #define. The set of features to enable is dictated by ShaderFeatures.
    class DrawProgram
    {
    public:
        DrawProgram(const DrawProgram&) = delete;
        DrawProgram& operator=(const DrawProgram&) = delete;
        DrawProgram(RenderContextGLImpl*,
                    PipelineCreateType,
                    gpu::DrawType,
                    gpu::ShaderFeatures,
                    gpu::InterlockMode,
                    gpu::ShaderMiscFlags
#ifdef WITH_RIVE_TOOLS
                    ,
                    bool synthesizeCompilationFailures
#endif
        );
        ~DrawProgram();

        GLuint id() const { return m_id; }
        GLint baseInstanceUniformLocation() const
        {
            return m_baseInstanceUniformLocation;
        }

        PipelineStatus status() const
        {
            switch (m_creationState)
            {
                case CreationState::waitingOnProgram:
                case CreationState::waitingOnShaders:
                    return PipelineStatus::notReady;

                case CreationState::complete:
                    return PipelineStatus::ready;

                case CreationState::error:
                    return PipelineStatus::errored;
            }

            RIVE_UNREACHABLE();
        }

        bool advanceCreation(RenderContextGLImpl*,
                             PipelineCreateType,
                             gpu::DrawType,
                             gpu::ShaderFeatures,
                             gpu::InterlockMode,
                             gpu::ShaderMiscFlags);

    private:
        enum class CreationState
        {
            waitingOnShaders,
            waitingOnProgram,
            complete,
            error,
        };

        DrawShader m_fragmentShader;
        const DrawShader* m_vertexShader = nullptr;
        CreationState m_creationState = CreationState::waitingOnShaders;
        GLuint m_id = 0;
        GLint m_baseInstanceUniformLocation = -1;
        const rcp<GLState> m_state;
    };

    class GLPipelineManager : public AsyncPipelineManager<DrawProgram>
    {
        using Super = AsyncPipelineManager<DrawProgram>;

    public:
        GLPipelineManager(ShaderCompilationMode, RenderContextGLImpl*);

    protected:
        virtual std::unique_ptr<DrawProgram> createPipeline(
            PipelineCreateType createType,
            uint32_t key,
            const PipelineProps&) override;

        virtual PipelineStatus getPipelineStatus(
            const DrawProgram& state) const override;

        virtual bool advanceCreation(DrawProgram&,
                                     const PipelineProps&) override;

    private:
        RenderContextGLImpl* m_context;
    };

    class GLVertexShaderManager : public VertexShaderManager<DrawShader>
    {
    public:
        GLVertexShaderManager(RenderContextGLImpl* context);

    protected:
        virtual DrawShader createVertexShader(gpu::DrawType,
                                              gpu::ShaderFeatures,
                                              gpu::InterlockMode) override;

    private:
        RenderContextGLImpl* m_context;
    };

    // Not all programs have a unique vertex shader, so we cache and reuse them
    // where possible.
    GLVertexShaderManager m_vsManager;
    GLPipelineManager m_pipelineManager;

    // Vertex/index buffers for drawing paths.
    glutils::VAO m_drawVAO;
    glutils::Buffer m_patchVerticesBuffer;
    glutils::Buffer m_patchIndicesBuffer;
    glutils::VAO m_trianglesVAO;

    // Vertex/index buffers for drawing image rects. (Atomic mode only.)
    glutils::VAO m_imageRectVAO;
    glutils::Buffer m_imageRectVertexBuffer;
    glutils::Buffer m_imageRectIndexBuffer;

    glutils::VAO m_imageMeshVAO;
    glutils::VAO m_emptyVAO;

    // Used for blitting non-MSAA -> MSAA, which isn't supported by
    // glBlitFramebuffer().
    glutils::Program m_blitAsDrawProgram = glutils::Program::Zero();

    const rcp<GLState> m_state;
};
} // namespace rive::gpu

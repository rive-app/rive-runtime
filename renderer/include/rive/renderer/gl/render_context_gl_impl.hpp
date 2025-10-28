/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/renderer/async_pipeline_manager.hpp"
#include "rive/renderer/gl/gl_state.hpp"
#include "rive/renderer/gl/gl_utils.hpp"
#include "rive/renderer/render_context_helper_impl.hpp"

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

    // Storage type and rendering method for the feather atlas.
    //
    // Ideally we would always use r32f or r16f, but floating point color
    // buffers are only supported via extensions in GL.
    //
    // These are sorted with the most preferred types higher up in the list.
    enum class AtlasType
    {
        r32f, // Most preferred. Uses HW blending to count coverage.
        r16f, // Uses HW blending but loses precision on complex feathers.

        r32uiFramebufferFetch, // Stores coverage as fp32 bits in a uint.
        r32uiPixelLocalStorage,

        r32iAtomicTexture, // Stores coverage as 16:16 fixed point.

        rgba8, // Low quality, but always supported. Uses HW blending and breaks
               // up coverage into all 4 components of an RGBA texture.
    };

    AtlasType atlasType() const { return m_atlasType; }

#ifdef WITH_RIVE_TOOLS
    // Changes the context's AtlasType for testing purposes. If atlasDesiredType
    // is not supported, the next supported AtlasType down the list is chosen.
    //
    // NOTE: this also calls releaseResources() on the owning RenderContext to
    // ensure the atlas texture gets reallocated.
    void testingOnly_resetAtlasDesiredType(RenderContext* owningRenderContext,
                                           AtlasType atlasDesiredType);
#endif

private:
    class DrawProgram;

    // Manages how we implement pixel local storage in shaders.
    class PixelLocalStorageImpl
    {
    public:
        virtual void init(rcp<GLState>) {}

        // Sets any supported interlock modes in PlatformFeatures to true.
        // Leaves the rest unchanged.
        virtual void getSupportedInterlockModes(const GLCapabilities&,
                                                PlatformFeatures*) const = 0;

        virtual void resizeTransientPLSBacking(uint32_t width,
                                               uint32_t height,
                                               uint32_t planeCount)
        {}
        virtual void resizeAtomicCoverageBacking(uint32_t width,
                                                 uint32_t height)
        {}

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

        // Certain PLS draws require implementation-specific pipeline state that
        // differs from the general pipeline state.
        virtual void applyPipelineStateOverrides(const DrawBatch&,
                                                 const FlushDescriptor&,
                                                 const PlatformFeatures&,
                                                 PipelineState*) const
        {}

        virtual void activatePixelLocalStorage(RenderContextGLImpl*,
                                               const FlushDescriptor&) = 0;
        virtual void deactivatePixelLocalStorage(RenderContextGLImpl*,
                                                 const FlushDescriptor&) = 0;

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

    void buildAtlasRenderPipelines();

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
    void resizeTransientPLSBacking(uint32_t width,
                                   uint32_t height,
                                   uint32_t planeCount) override;
    void resizeAtomicCoverageBacking(uint32_t width, uint32_t height) override;

    void preBeginFrame(RenderContext*) override;

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

    // Renders feathers to the atlas texture.
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

    // Atlas rendering pipelines.
    AtlasType m_atlasType;
    glutils::Shader m_atlasVertexShader;
    AtlasProgram m_atlasFillProgram;
    AtlasProgram m_atlasStrokeProgram;
    gpu::PipelineState m_atlasFillPipelineState;
    gpu::PipelineState m_atlasStrokePipelineState;
#ifdef RIVE_ANDROID
    // Pipelines for clearing and resolving EXT_shader_pixel_local_storage.
    glutils::Shader m_atlasResolveVertexShader;
    glutils::Program m_atlasClearProgram = glutils::Program::Zero();
    glutils::Program m_atlasResolveProgram = glutils::Program::Zero();
    glutils::VAO m_atlasResolveVAO;
#endif
    glutils::Texture m_atlasTexture = glutils::Texture::Zero();
    glutils::Framebuffer m_atlasFBO;

    // Wraps a compiled GL "draw" shader, either vertex or fragment, with a
    // specific set of features enabled via #define. The set of features to
    // enable is dictated by ShaderFeatures.
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
                   gpu::ShaderFeatures shaderFeatures,
                   gpu::InterlockMode interlockMode,
                   gpu::ShaderMiscFlags shaderMiscFlags);

        ~DrawShader();

        GLuint id() const { return m_id; }

    private:
        GLuint m_id = 0;
    };

    // Wraps a compiled and linked GL "draw" program, with a specific set of
    // features enabled via #define. The set of features to enable is dictated
    // by ShaderFeatures.
    class DrawProgram
    {
    public:
        using PipelineProps = StandardPipelineProps;
        using VertexShaderType = DrawShader;
        using FragmentShaderType = DrawShader;

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
                    SynthesizedFailureType
#endif
        );
        ~DrawProgram();

        GLuint id() const { return m_id; }
        GLint baseInstanceUniformLocation() const
        {
            return m_baseInstanceUniformLocation;
        }

        PipelineStatus status() const { return m_pipelineStatus; }

        bool advanceCreation(RenderContextGLImpl*,
                             PipelineCreateType,
                             gpu::DrawType,
                             gpu::ShaderFeatures,
                             gpu::InterlockMode,
                             gpu::ShaderMiscFlags);

    private:
        const DrawShader* m_fragmentShader = nullptr;
        const DrawShader* m_vertexShader = nullptr;
        PipelineStatus m_pipelineStatus = PipelineStatus::notReady;
        GLuint m_id = 0;
        GLint m_baseInstanceUniformLocation = -1;
        const rcp<GLState> m_state;
#ifdef WITH_RIVE_TOOLS
        SynthesizedFailureType m_synthesizedFailureType =
            SynthesizedFailureType::none;
#endif
    };

    class GLPipelineManager : public AsyncPipelineManager<DrawProgram>
    {
        using Super = AsyncPipelineManager<DrawProgram>;

    public:
        GLPipelineManager(ShaderCompilationMode, RenderContextGLImpl*);

    protected:
        virtual std::unique_ptr<DrawShader> createVertexShader(
            DrawType,
            ShaderFeatures,
            InterlockMode) override;

        virtual std::unique_ptr<DrawShader> createFragmentShader(
            DrawType,
            ShaderFeatures,
            InterlockMode,
            ShaderMiscFlags) override;

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

    bool m_testForAdvancedBlendError = false;
};
} // namespace rive::gpu

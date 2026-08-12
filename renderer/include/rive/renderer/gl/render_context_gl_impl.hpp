/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/renderer/async_pipeline_manager.hpp"
#include "rive/renderer/gl/gl_state.hpp"
#include "rive/renderer/gl/gl_utils.hpp"
#include "rive/renderer/render_context_helper_impl.hpp"

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>

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
                                  GPUTextureFormat format,
                                  const uint8_t imageData[],
                                  uint8_t blockWidth = 1,
                                  uint8_t blockHeight = 1,
                                  bool srgb = false,
                                  bool generateRemainingMips = false) override;

    // Takes ownership of textureID and responsibility for deleting it.
    rcp<Texture> adoptImageTexture(uint32_t width,
                                   uint32_t height,
                                   GLuint textureID);

#ifdef RIVE_CANVAS
    rcp<RenderCanvas> makeRenderCanvas(uint32_t width,
                                       uint32_t height) override;

    // Creates a shell canvas with no texture; the deferred replay worker
    // backs it on its own context via ensureCanvasBacking.
    rcp<RenderCanvas> makeDeferredRenderCanvas(uint32_t width,
                                               uint32_t height) override;

    void ensureCanvasBacking(gpu::RenderCanvas* canvas) override;

    std::unique_ptr<rive::ore::Context> makeOreContext() override;

    // GL-only: returns a Y-flipped companion of a Rive 2D RenderCanvas
    // texture, lazily allocating it on first call. Returns nullptr if
    // `sourceTex` is not a canvas target (i.e. it's a regular image).
    rcp<RiveRenderImage> getCanvasImportMirror(gpu::Texture* sourceTex,
                                               uint32_t width,
                                               uint32_t height);

    // ── Canvas mirror registry (GL/WebGL only) ─────────────────────────
    //
    // Implements the "imported canvas mirror" pattern for the Rive 2D
    // RenderCanvas → Ore boundary. On GL the PLS-rendered canvas is
    // bottom-up in memory; consumers that sample it from a WGSL shader
    // need a top-up companion texture. The registry tracks the source
    // canvas GLuint, lazily allocates a companion + a pair of FBOs the
    // first time the canvas is imported, and arranges for the companion
    // to be Y-flip-blitted at the end of the source canvas's own flush()
    // (when GL state is clean).
    //
    // Lifetime: registerCanvasTarget is called from makeRenderCanvas;
    // unregisterCanvasTarget is called from the canvas-target texture's
    // destructor (CanvasTargetTextureGLImpl). The entry owns the
    // companion, so every import of one source shares one companion and
    // it outlives any single view of it.

    void registerCanvasTarget(GLuint sourceTex);
    void unregisterCanvasTarget(GLuint sourceTex);

    // A deferred canvas is dropped by the thread that recorded it, so its entry
    // and the FBOs in it come down on this context's own thread instead.
    void releaseCanvasTarget(GLuint sourceTex);

    // Looks up an existing mirror for `sourceTex` and allocates one if
    // none exists yet. Returns nullptr if `sourceTex` was never registered
    // (i.e. is not a canvas target — caller should fall through to a
    // direct view of the source).
    rcp<RiveRenderImage> getOrCreateCanvasMirror(GLuint sourceTex,
                                                 uint32_t width,
                                                 uint32_t height);

    // Called from RenderContextGLImpl::flush after the post-flush
    // glFlush() barrier. Walks the registry for `targetTex`; if there is
    // a registered entry with a non-null mirror, runs the Y-flip blit
    // from source → mirror. Cheap O(1) hash lookup with a negative-case
    // early-out — non-canvas targets pay nothing.
    void blitMirrorIfRegistered(GLuint targetTex);
#endif

    // Called *after* the GL context has been modified externally.
    // Re-binds Rive internal resources and invalidates the internal cache of GL
    // state.
    void invalidateGLState();

    // Sampler objects and texture unit bindings are global state no FBO or VAO
    // restores, so Ore's leftovers would override our sampling parameters.
    void scrubStateAfterOre() override;

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
    enum class FeatherAtlasRenderType
    {
        r16f, // Most preferred. Balances performances with precision.
        r32f, // Uses HW blending to count coverage.

        r32uiFramebufferFetch, // Stores coverage as fp32 bits in a uint.
        r8PixelLocalStorageEXT,
        r32uiPixelLocalStorageANGLE,

        r32iAtomicTexture, // Stores coverage as 16:16 fixed point.

        rgba8, // Low quality, but always supported. Uses HW blending and breaks
               // up coverage into all 4 components of an RGBA texture.
    };

    FeatherAtlasRenderType featherAtlasRenderType() const
    {
        return m_featherAtlasRenderType;
    }

#ifdef WITH_RIVE_TOOLS
    // Changes the context's FeatherAtlasRenderType for testing purposes. If
    // atlasDesiredRenderType is not supported, the next supported
    // AtlasRenderType down the list is chosen.
    //
    // Returns the original FeatherAtlasRenderType from before this call was
    // made.
    //
    // NOTE: this also calls releaseResources() on the owning RenderContext to
    // ensure the atlas texture gets reallocated.
    FeatherAtlasRenderType testingOnly_resetFeatherAtlasDesiredRenderType(
        RenderContext* owningRenderContext,
        FeatherAtlasRenderType desiredRenderType);

    bool testingOnly_setBlendAdvancedCoherentKHRSupported(bool supported);
    bool testingOnly_setBlendAdvancedKHRSupported(bool supported);
#endif

private:
#ifdef RIVE_CANVAS
    // Shared canvas wiring; `tex` of 0 makes an unbacked shell canvas.
    rcp<RenderCanvas> wrapCanvasBacking(uint32_t width,
                                        uint32_t height,
                                        GLuint tex);
#endif

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

    void buildFeatherAtlasRenderPipelines();

    std::unique_ptr<BufferRing> makeUniformBufferRing(
        size_t capacityInBytes) override;
    std::unique_ptr<BufferRing> makeStorageBufferRing(
        size_t capacityInBytes,
        gpu::StorageBufferStructure) override;
    std::unique_ptr<BufferRing> makeVertexBufferRing(
        size_t capacityInBytes) override;

    void resizeGradientTexture(uint32_t width, uint32_t height) override;
    void resizeTessellationTexture(uint32_t width, uint32_t height) override;
    void resizeFeatherAtlasTexture(uint32_t width, uint32_t height) override;
    void resizeTransientPLSBacking(uint32_t width,
                                   uint32_t height,
                                   uint32_t planeCount) override;
    void resizeAtomicCoverageBacking(uint32_t width, uint32_t height) override;

    void preBeginFrame(RenderContext*) override;

    void flush(const FlushDescriptor&) override;

    // We have observed Adreno 308 crash when drawing too many instances spread
    // across any number of draw calls. This class breaks them up with glFlush
    // in order to fix the crashes.
    class GLFlushInjector
    {
    public:
        GLFlushInjector(const GLCapabilities& capabilities) :
            m_maxSupportedInstancesPerFlush(
                capabilities.maxSupportedInstancesPerFlush)
        {}

        void flushBeforeInstancedDrawIfNeeded(uint32_t nextInstanceCount)
        {
            if (m_currentFlushInstanceCount + nextInstanceCount >
                m_maxSupportedInstancesPerFlush)
            {
                glFlush();
                m_currentFlushInstanceCount = 0;
            }
            m_currentFlushInstanceCount += nextInstanceCount;
        }

    private:
        const uint32_t m_maxSupportedInstancesPerFlush;
        uint32_t m_currentFlushInstanceCount = 0;
    };

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
        GLint baseInstanceUniformLocation,
        GLFlushInjector*);

    GLCapabilities m_capabilities;

    std::unique_ptr<PixelLocalStorageImpl> m_plsImpl;

    // Gradient texture rendering.
    glutils::Program m_colorRampProgram;
    glutils::VAO m_colorRampVAO;
    glutils::Framebuffer m_colorRampFBO;
    GLuint m_gradientTexture = 0;

    // Gaussian integral table for feathering.
    glutils::Texture m_gaussianIntegralTexture;

    // Tessellation texture rendering.
    glutils::Program m_tessellateProgram;
    glutils::VAO m_tessellateVAO;
    glutils::Buffer m_tessSpanIndexBuffer;
    glutils::Framebuffer m_tessellateFBO;
    GLuint m_tessVertexTexture = 0;

    // Renders feathers to the feather atlas texture.
    class FeatherAtlasProgram
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
    FeatherAtlasRenderType m_featherAtlasRenderType;
    glutils::Shader m_featherAtlasVertexShader;
    FeatherAtlasProgram m_featherAtlasFillProgram;
    FeatherAtlasProgram m_featherAtlasStrokeProgram;
    gpu::PipelineState m_featherAtlasFillPipelineState;
    gpu::PipelineState m_featherAtlasStrokePipelineState;
    // Pipelines for clearing and resolving atlases into a GL_R8 texture for
    // sampling.
    glutils::Shader m_featherAtlasResolveVertexShader;
    glutils::Program m_featherAtlasClearProgram = glutils::Program::Zero();
    glutils::Program m_featherAtlasResolveProgram = glutils::Program::Zero();
    glutils::VAO m_featherAtlasResolveVAO;
    glutils::Texture m_featherAtlasRenderTexture = glutils::Texture::Zero();
    glutils::Texture m_featherAtlasTexture = glutils::Texture::Zero();
    glutils::Framebuffer m_featherAtlasRenderFBO = glutils::Framebuffer::Zero();
    glutils::Framebuffer m_featherAtlasResolveFBO =
        glutils::Framebuffer::Zero();

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
            const PipelineProps&,
            const PlatformFeatures&) override;

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

#ifdef RIVE_CANVAS
    // Imported canvas mirror registry. See registerCanvasTarget /
    // getOrCreateCanvasMirror / blitMirrorIfRegistered above.
    struct CanvasMirrorEntry
    {
        // An entry exists for every PLS canvas target; the source GLuint is
        // the hash key. Lazily allocated by getOrCreateCanvasMirror, owned
        // here so repeat imports of one canvas share a single companion.
        rcp<RiveRenderImage> mirrorImage;
        uint32_t width = 0;
        uint32_t height = 0;

        // Persistent FBOs that the blit reuses every frame. Allocated
        // when the mirror is first created. Released when the entry's
        // owning canvas is unregistered.
        GLuint readFBO = 0;
        GLuint drawFBO = 0;
    };
    std::unordered_map<GLuint, CanvasMirrorEntry> m_canvasMirrors;

    // Canvas targets released off this context's thread, drained by flush.
    std::mutex m_releasedCanvasTargetMutex;
    std::vector<GLuint> m_releasedCanvasTargets;
    std::atomic<bool> m_hasReleasedCanvasTargets{false};
    const glutils::GLContextID m_glContext = glutils::CurrentContextID();

    void drainReleasedCanvasTargets();
#endif
};
} // namespace rive::gpu

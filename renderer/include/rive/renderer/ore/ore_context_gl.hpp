/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/ore/ore_context.hpp"

#include <unordered_map>

// Note: load_gles_extensions.hpp (glad) is intentionally NOT included here.
// The private GL state only needs 'int' (GLint is always int), keeping this
// header free of glad so it can be included without glad in the search path.

namespace rive::ore
{

class RenderPassGL;
class BindGroupGL;
class TextureGL;

class ContextGL : public Context
{
public:
    // renderContextImpl is the RenderContextGLImpl that owns this context's
    // canvases, needed for the Y flip import mirror. Null on standalone GMs.
    static std::unique_ptr<ContextGL> Make(void* renderContextImpl = nullptr);

    ~ContextGL() override;

    /**
     * Discards framebuffer objects cached by completed Ore render passes.
     *
     * This must be called with the owning GL context current and with no Ore
     * render pass active. Cached VAOs are unaffected because they do not
     * reference framebuffer or drawable state.
     */
    void invalidateScratchFramebuffers();

    rcp<Buffer> makeBuffer(const BufferDesc& desc) override;
    rcp<Texture> makeTexture(const TextureDesc& desc) override;
    rcp<TextureView> makeTextureView(const TextureViewDesc& desc) override;
    rcp<Sampler> makeSampler(const SamplerDesc& desc) override;
    rcp<ShaderModule> makeShaderModule(const ShaderModuleDesc& desc) override;
    rcp<BindGroupLayout> makeBindGroupLayout(
        const BindGroupLayoutDesc& desc) override;
    rcp<Pipeline> makePipeline(const PipelineDesc& desc,
                               std::string* outError = nullptr) override;
    rcp<BindGroup> makeBindGroup(const BindGroupDesc& desc) override;

    std::unique_ptr<RenderPass> beginRenderPass(
        const RenderPassDesc& desc,
        std::string* outError = nullptr) override;

    void beginFrame(const FrameDescriptor&) override;
    void endFrame() override;
    void waitForGPU() override;

    // GL stays on per pass inline replay: it has no command buffer so no
    // natural frame boundary drain, and the ore frame is not reliably driven.
    // TODO: whole frame GL deferral.
    bool usesDeferredFrameReplay() const override { return false; }

    rcp<TextureView> wrapCanvasTexture(gpu::RenderCanvas* canvas) override;
    rcp<TextureView> wrapCanvasSampleView(gpu::RenderCanvas* canvas) override;
    rcp<TextureView> wrapRiveTexture(gpu::Texture* gpuTex,
                                     uint32_t width,
                                     uint32_t height) override;

    ShaderTarget shaderTarget() const override { return ShaderTarget::glsl; }

    ContextGL(const ContextGL&) = delete;
    ContextGL& operator=(const ContextGL&) = delete;

private:
    friend class RenderPassGL;
    friend class BindGroupGL;
    friend class TextureGL;

    explicit ContextGL(void* renderContextImpl) :
        Context(nullptr), m_renderContextImpl(renderContextImpl)
    {}

    // Borrowed RenderContextGLImpl, void* to avoid the header dependency.
    void* m_renderContextImpl = nullptr;

    // ── Scratch pass objects ───────────────────────────────────────────
    //
    // A pass's FBO and VAO carry nothing between passes: the FBO is
    // reattached from the descriptor every time and the VAO exists only to
    // keep Ore's attrib state off the host renderer's. Minting a pair per
    // pass therefore burns two GL names per pass, and on WebGL emscripten
    // never recycles a name, so a replayed frame ratchets GL.framebuffers
    // and GL.vaos for the life of the page. One of each per context is
    // enough, because Ore has exactly one pass open at a time. A pass that
    // opens while the pair is out on loan still mints its own, so nesting
    // stays correct rather than aliasing one FBO across two passes.
    //
    // These are GLuints; the header stays free of glad (see note above).
    unsigned int acquireScratchFBO();
    unsigned int acquireScratchVAO();
    unsigned int scratchResolveFBO();
    void releaseScratchFBO(uint32_t colorCount, unsigned int depthAttachment);
    void releaseScratchVAO();

    unsigned int m_scratchFBO = 0;
    unsigned int m_scratchVAO = 0;
    unsigned int m_scratchResolveFBO = 0;
    bool m_scratchFBOLent = false;
    bool m_scratchVAOLent = false;
    // What the last borrower attached, so the next one is handed an FBO as
    // empty as a freshly minted one and cannot inherit a stale attachment.
    uint32_t m_scratchFBOColorCount = 0;
    unsigned int m_scratchFBODepthAttachment = 0; // GLenum, 0 when none

    // GL state tracking for save/restore at frame boundaries.
    // NOTE: GL_ELEMENT_ARRAY_BUFFER is intentionally excluded — it is VAO
    // state, so restoring the VAO implicitly restores the element buffer.
    struct GLSavedState
    {
        int program = 0;
        int arrayBuffer = 0;
        int uniformBuffer = 0;
        int framebuffer = 0;
        int vertexArray = 0;
    } m_savedState;
};

} // namespace rive::ore

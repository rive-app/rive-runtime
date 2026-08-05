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

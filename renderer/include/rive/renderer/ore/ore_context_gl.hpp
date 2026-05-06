/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/ore/ore_context.hpp"

// Note: load_gles_extensions.hpp (glad) is intentionally NOT included here.
// The private GL state only needs 'int' (GLint is always int), keeping this
// header free of glad so it can be included without glad in the search path.

namespace rive::ore
{

class ContextGL : public Context
{
public:
    static std::unique_ptr<ContextGL> Make();

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

    RenderPass beginRenderPass(const RenderPassDesc& desc,
                               std::string* outError = nullptr) override;

    void beginFrame() override;
    void endFrame() override;
    void waitForGPU() override;

    rcp<TextureView> wrapCanvasTexture(gpu::RenderCanvas* canvas) override;
    rcp<TextureView> wrapRiveTexture(gpu::Texture* gpuTex,
                                     uint32_t width,
                                     uint32_t height) override;

    ContextGL(const ContextGL&) = delete;
    ContextGL& operator=(const ContextGL&) = delete;

private:
    friend class RenderPass;
    friend class BindGroup;
    friend class Texture;

    ContextGL() = default;

    // GL state tracking for save/restore at frame boundaries.
    // NOTE: GL_ELEMENT_ARRAY_BUFFER is intentionally excluded — it is VAO
    // state, so restoring the VAO implicitly restores the element buffer.
    struct GLSavedState
    {
        int program = 0;
        int arrayBuffer = 0;
        int framebuffer = 0;
        int vertexArray = 0;
    } m_savedState;
};

} // namespace rive::ore

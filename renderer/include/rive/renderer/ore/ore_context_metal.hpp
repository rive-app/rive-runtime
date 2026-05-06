/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/ore/ore_context.hpp"

#import <Metal/Metal.h>

namespace rive::ore
{

class ContextMetal : public Context
{
public:
    static std::unique_ptr<ContextMetal> Make(id<MTLDevice> device,
                                              id<MTLCommandQueue> queue);

    ~ContextMetal() override;

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

    ContextMetal(const ContextMetal&) = delete;
    ContextMetal& operator=(const ContextMetal&) = delete;

private:
    friend class RenderPass;
    friend class BindGroup;
    friend class Texture;

    ContextMetal() = default;

    // Metal implementation helpers — defined in ore_context_metal.mm.
    // The public make*/begin*/wrap* overrides delegate to these.
    void mtlPopulateFeatures(id<MTLDevice> device);
    rcp<Buffer> mtlMakeBuffer(const BufferDesc& desc);
    rcp<Texture> mtlMakeTexture(const TextureDesc& desc);
    rcp<TextureView> mtlMakeTextureView(const TextureViewDesc& desc);
    rcp<Sampler> mtlMakeSampler(const SamplerDesc& desc);
    rcp<ShaderModule> mtlMakeShaderModule(const ShaderModuleDesc& desc);
    rcp<Pipeline> mtlMakePipeline(const PipelineDesc& desc,
                                  std::string* outError);
    rcp<BindGroup> mtlMakeBindGroup(const BindGroupDesc& desc);
    RenderPass mtlBeginRenderPass(const RenderPassDesc& desc,
                                  std::string* outError);
    rcp<TextureView> mtlWrapCanvasTexture(gpu::RenderCanvas* canvas);

    id<MTLDevice> m_mtlDevice = nil;
    id<MTLCommandQueue> m_mtlQueue = nil;
    id<MTLCommandBuffer> m_mtlCommandBuffer = nil;
};

} // namespace rive::ore

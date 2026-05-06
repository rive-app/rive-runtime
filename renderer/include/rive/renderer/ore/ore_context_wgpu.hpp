/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/ore/ore_context.hpp"

#include <webgpu/webgpu_cpp.h>
#ifndef RIVE_DAWN
#include <webgpu/webgpu_wagyu.h>
#endif

namespace rive::ore
{

class ContextWGPU : public Context
{
public:
    // backendType is wgpu::BackendType::OpenGLES or wgpu::BackendType::Vulkan,
    // obtained from RenderContextWebGPUImpl::capabilities().backendType.
    static std::unique_ptr<ContextWGPU> Make(wgpu::Device device,
                                             wgpu::Queue queue,
                                             wgpu::BackendType backendType);

    ~ContextWGPU() override;

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

    // External-encoder mode: Ore records into the host's wgpu::CommandEncoder
    // (already created by the caller) instead of owning its own. The host
    // retains ownership of Finish()/Submit() and frame sequencing. Unlike
    // Vulkan and D3D12, no deferred-destroy queue is needed: wgpu handles are
    // refcounted and command encoders / buffers pin referenced resources
    // until GPU completion, so dropping the C++ wrapper while the encoder
    // still references the resource is safe by WebGPU semantics.
    //
    // Enables cross-engine read-after-write (e.g. Rive renders into a canvas
    // then Ore samples it) that the owned-encoder model cannot support —
    // same motivation as the VK/D3D12 external overloads.
    void beginFrame(wgpu::CommandEncoder externalEncoder);

    // Returns true when the runtime target is OpenGL ES (wagyu GLSLRAW path).
    // Use this to select between GLES and Vulkan GLSL shader source.
    bool isGLES() const { return m_wgpuBackend == WGPUBackend::OpenGLES; }

    ContextWGPU(const ContextWGPU&) = delete;
    ContextWGPU& operator=(const ContextWGPU&) = delete;

private:
    friend class RenderPass;
    friend class BindGroup;
    friend class Texture;

    ContextWGPU() = default;

    enum class WGPUBackend
    {
        OpenGLES,
        Vulkan
    };
    WGPUBackend m_wgpuBackend = WGPUBackend::Vulkan;
    wgpu::Device m_wgpuDevice;
    wgpu::Queue m_wgpuQueue;
    wgpu::CommandEncoder m_wgpuCommandEncoder;
    // True while the current frame is recording into a host-provided encoder.
    // endFrame() skips Finish()/Submit() when set.
    bool m_wgpuExternalEncoder = false;
};

} // namespace rive::ore

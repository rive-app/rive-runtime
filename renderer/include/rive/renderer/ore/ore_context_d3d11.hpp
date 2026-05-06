/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/ore/ore_context.hpp"

#include <d3d11.h>
#include <d3d11_1.h> // ID3D11DeviceContext1 for offset-range CB binds
#include <wrl/client.h>

namespace rive::ore
{

class ContextD3D11 : public Context
{
public:
    static std::unique_ptr<ContextD3D11> Make(ID3D11Device* device,
                                              ID3D11DeviceContext* context);

    ~ContextD3D11() override;

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

    ContextD3D11(const ContextD3D11&) = delete;
    ContextD3D11& operator=(const ContextD3D11&) = delete;

private:
    friend class RenderPass;
    friend class BindGroup;
    friend class Texture;

    ContextD3D11() = default;

    // D3D11 implementation helpers — defined in ore_context_d3d11.cpp.
    rcp<Buffer> d3d11MakeBuffer(const BufferDesc& desc);
    rcp<Texture> d3d11MakeTexture(const TextureDesc& desc);
    rcp<TextureView> d3d11MakeTextureView(const TextureViewDesc& desc);
    rcp<Sampler> d3d11MakeSampler(const SamplerDesc& desc);
    rcp<ShaderModule> d3d11MakeShaderModule(const ShaderModuleDesc& desc);
    rcp<Pipeline> d3d11MakePipeline(const PipelineDesc& desc,
                                    std::string* outError);
    rcp<BindGroup> d3d11MakeBindGroup(const BindGroupDesc& desc);
    rcp<BindGroupLayout> d3d11MakeBindGroupLayout(
        const BindGroupLayoutDesc& desc);
    RenderPass d3d11BeginRenderPass(const RenderPassDesc& desc,
                                    std::string* outError);
    rcp<TextureView> d3d11WrapCanvasTexture(gpu::RenderCanvas* canvas);
    rcp<TextureView> d3d11WrapRiveTexture(gpu::Texture* gpuTex,
                                          uint32_t w,
                                          uint32_t h);

    Microsoft::WRL::ComPtr<ID3D11Device> m_d3d11Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3d11Context;
    // 11.1 context for offset-range constant-buffer binds
    // (VSSetConstantBuffers1 / PSSetConstantBuffers1). Acquired via QI at
    // Make(); nullptr on pre-11.1 drivers, in which case static-offset and
    // dynamic-offset UBOs fall back to whole-buffer binds.
    Microsoft::WRL::ComPtr<ID3D11DeviceContext1> m_d3d11Context1;
};

} // namespace rive::ore

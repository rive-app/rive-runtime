/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/ore/ore_context.hpp"

#include <d3d12.h>
#include <wrl/client.h>
#include <functional>
#include <vector>

namespace rive::ore
{
class ContextD3D12 : public Context
{
public:
    // Takes the D3D12 device and command queue. Ore shares the caller's
    // queue so that canvas-texture renders are visible to the host renderer
    // without cross-queue synchronization.
    static std::unique_ptr<ContextD3D12> Make(
        rcp<rive::gpu::GPUResourceManager> manager,
        ID3D12Device* device);

    ~ContextD3D12() override;

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

    rcp<TextureView> wrapCanvasTexture(gpu::RenderCanvas* canvas) override;
    rcp<TextureView> wrapRiveTexture(gpu::Texture* gpuTex,
                                     uint32_t width,
                                     uint32_t height) override;

    ShaderTarget shaderTarget() const override { return ShaderTarget::hlsl; }

    ContextD3D12(const ContextD3D12&) = delete;
    ContextD3D12& operator=(const ContextD3D12&) = delete;

private:
    friend class RenderPassD3D12;
    friend class BindGroupD3D12;
    friend class TextureD3D12;

    ContextD3D12(rcp<rive::gpu::GPUResourceManager> manager) :
        Context(std::move(manager))
    {}

    // D3D12 implementation helpers — defined in ore_context_d3d12.cpp.
    rcp<Buffer> d3d12MakeBuffer(const BufferDesc& desc);
    rcp<Texture> d3d12MakeTexture(const TextureDesc& desc);
    rcp<TextureView> d3d12MakeTextureView(const TextureViewDesc& desc);
    rcp<Sampler> d3d12MakeSampler(const SamplerDesc& desc);
    rcp<ShaderModule> d3d12MakeShaderModule(const ShaderModuleDesc& desc);
    rcp<Pipeline> d3d12MakePipeline(const PipelineDesc& desc,
                                    std::string* outError);
    rcp<BindGroup> d3d12MakeBindGroup(const BindGroupDesc& desc);
    rcp<BindGroupLayout> d3d12MakeBindGroupLayout(
        const BindGroupLayoutDesc& desc);
    std::unique_ptr<RenderPass> d3d12BeginRenderPass(const RenderPassDesc& desc,
                                                     std::string* outError);
    rcp<TextureView> d3d12WrapCanvasTexture(gpu::RenderCanvas* canvas);
    rcp<TextureView> d3d12WrapRiveTexture(gpu::Texture* gpuTex,
                                          uint32_t w,
                                          uint32_t h);

    Microsoft::WRL::ComPtr<ID3D12Device> m_d3dDevice;
    // Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_d3dQueue;
    //  Active command list for the current frame. Points at m_d3dOwnedCmdList
    //  in owned-CL mode and at the host-provided command list in external-CL
    //  mode. All recording code reads through this pointer, so the two modes
    //  share one code path.
    ID3D12GraphicsCommandList* m_d3dCmdList = nullptr;
    // resource-creation time.
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_d3dCpuSrvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_d3dCpuRtvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_d3dCpuDsvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_d3dCpuSamplerHeap;
    UINT m_d3dCpuSrvAllocated = 0;
    UINT m_d3dCpuRtvAllocated = 0;
    UINT m_d3dCpuDsvAllocated = 0;
    UINT m_d3dCpuSamplerAllocated = 0;
    UINT m_d3dSrvDescSize = 0;
    UINT m_d3dRtvDescSize = 0;
    UINT m_d3dDsvDescSize = 0;
    UINT m_d3dSamplerDescSize = 0;
    // Null descriptors for unused descriptor-table slots.
    D3D12_CPU_DESCRIPTOR_HANDLE m_d3dNullSrv = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_d3dNullSampler = {};
    // GPU-visible heaps: reset at beginFrame, allocated linearly per draw.
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_d3dGpuSrvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_d3dGpuSamplerHeap;
    UINT m_d3dGpuSrvAllocated = 0;
    UINT m_d3dGpuSamplerAllocated = 0;
    // Helpers called by RenderPass to allocate and resolve descriptor handles.
    UINT d3d12AllocGpuSrvSlots(UINT count);
    UINT d3d12AllocGpuSamplerSlots(UINT count);
    D3D12_CPU_DESCRIPTOR_HANDLE d3d12GpuSrvCpuHandle(UINT index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE d3d12GpuSrvGpuHandle(UINT index) const;
    D3D12_CPU_DESCRIPTOR_HANDLE d3d12GpuSamplerCpuHandle(UINT index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE d3d12GpuSamplerGpuHandle(UINT index) const;
};

} // namespace rive::ore

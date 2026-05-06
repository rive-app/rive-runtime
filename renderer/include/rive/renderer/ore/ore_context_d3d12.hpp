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
    static std::unique_ptr<ContextD3D12> Make(ID3D12Device* device,
                                              ID3D12CommandQueue* queue);

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

    RenderPass beginRenderPass(const RenderPassDesc& desc,
                               std::string* outError = nullptr) override;

    void beginFrame() override;
    void endFrame() override;
    void waitForGPU() override;

    rcp<TextureView> wrapCanvasTexture(gpu::RenderCanvas* canvas) override;
    rcp<TextureView> wrapRiveTexture(gpu::Texture* gpuTex,
                                     uint32_t width,
                                     uint32_t height) override;

    // External-CL mode: Ore records into the host's
    // ID3D12GraphicsCommandList (already reset and open) instead of owning
    // its own. The host retains ownership of Close/ExecuteCommandLists/the
    // frame fence. Contract: the host must have waited on the prior frame's
    // completion fence before calling beginFrame(externalCl) again, so
    // deferred-destroy closures can be drained safely. Enables cross-engine
    // read-after-write (Rive writes, Ore reads) that the owned-CL model
    // cannot support.
    void beginFrame(ID3D12GraphicsCommandList* externalCl);

    // Called by Ore resource onRefCntReachedZero() implementations. If the
    // context is in external-CL mode, the closure is queued and drained at
    // the next beginFrame(), after the host has waited on the prior frame
    // fence. In owned-CL mode the closure runs immediately — Ore's endFrame()
    // does its own ExecuteCommandLists + WaitForFence, so by the time the
    // next owned frame begins, any objects referenced by the previous frame's
    // CL are safe to release.
    void d3dDeferDestroy(std::function<void()> destroy);

    ContextD3D12(const ContextD3D12&) = delete;
    ContextD3D12& operator=(const ContextD3D12&) = delete;

private:
    friend class RenderPass;
    friend class BindGroup;
    friend class Texture;

    ContextD3D12() = default;

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
    RenderPass d3d12BeginRenderPass(const RenderPassDesc& desc,
                                    std::string* outError);
    rcp<TextureView> d3d12WrapCanvasTexture(gpu::RenderCanvas* canvas);
    rcp<TextureView> d3d12WrapRiveTexture(gpu::Texture* gpuTex,
                                          uint32_t w,
                                          uint32_t h);

    Microsoft::WRL::ComPtr<ID3D12Device> m_d3dDevice;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_d3dQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_d3dAllocator;
    // ComPtr that owns the command list Ore allocates for owned-CL frames.
    // Not used directly by recording code — see m_d3dCmdList below.
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_d3dOwnedCmdList;
    // Active command list for the current frame. Points at m_d3dOwnedCmdList
    // in owned-CL mode and at the host-provided command list in external-CL
    // mode. All recording code reads through this pointer, so the two modes
    // share one code path.
    ID3D12GraphicsCommandList* m_d3dCmdList = nullptr;
    // Separate allocator/list for texture uploads (flushed at beginFrame).
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_d3dUploadAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_d3dUploadCmdList;
    bool m_d3dUploadListOpen = false;
    Microsoft::WRL::ComPtr<ID3D12Fence> m_d3dFence;
    uint64_t m_d3dFenceValue = 0;
    HANDLE m_d3dFenceEvent = nullptr;
    // CPU-visible descriptor heaps for creating descriptors at
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
    // Upload buffers kept alive until the frame fence-wait completes.
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_d3dPendingUploads;
    // True while the current frame is recording into a host-provided command
    // list. endFrame() skips Close/Execute/Wait when set; next beginFrame()
    // drains deferred destroys.
    bool m_d3dExternalCmdList = false;
    // Deferred-destroy closures (pipelines, samplers, buffers, textures,
    // views) queued while m_d3dExternalCmdList is true, drained at the next
    // beginFrame() or in the destructor after a GPU idle.
    std::vector<std::function<void()>> m_d3dDeferredDestroys;
    // Drain the deferred-destroy queue. Caller is responsible for ensuring
    // the prior GPU work has completed (via our fence or the host's).
    void d3dDrainDeferred();
    // Helpers called by RenderPass to allocate and resolve descriptor handles.
    UINT d3d12AllocGpuSrvSlots(UINT count);
    UINT d3d12AllocGpuSamplerSlots(UINT count);
    D3D12_CPU_DESCRIPTOR_HANDLE d3d12GpuSrvCpuHandle(UINT index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE d3d12GpuSrvGpuHandle(UINT index) const;
    D3D12_CPU_DESCRIPTOR_HANDLE d3d12GpuSamplerCpuHandle(UINT index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE d3d12GpuSamplerGpuHandle(UINT index) const;
    // Flush any open upload command list (called from beginFrame).
    void d3d12FlushUploads();
};

} // namespace rive::ore

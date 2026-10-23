/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/ore/ore_context.hpp"
#include "rive/renderer/d3d12/d3d12_utils.hpp"

#include <d3d12.h>
#include <wrl/client.h>
#include <climits>
#include <functional>
#include <vector>

namespace rive::ore
{
class D3D12DescriptorSlots;

// One slot in a CPU descriptor heap, given back when this goes away.
struct D3D12DescriptorSlot
{
  rcp<D3D12DescriptorSlots> pool;
  UINT index = UINT_MAX;

  D3D12DescriptorSlot() = default;
  D3D12DescriptorSlot(rcp<D3D12DescriptorSlots> p, UINT i)
      : pool(std::move(p))
      , index(i)
  {}
  D3D12DescriptorSlot(D3D12DescriptorSlot&& other) noexcept
      : pool(std::move(other.pool))
      , index(other.index)
  {
    other.index = UINT_MAX;
  }
  D3D12DescriptorSlot& operator=(D3D12DescriptorSlot&& other) noexcept;
  D3D12DescriptorSlot(const D3D12DescriptorSlot&) = delete;
  D3D12DescriptorSlot& operator=(const D3D12DescriptorSlot&) = delete;
  ~D3D12DescriptorSlot() { release(); }

  explicit operator bool() const { return pool != nullptr; }
  D3D12_CPU_DESCRIPTOR_HANDLE handle() const;
  void release();
};

// Slots in CPU descriptor heaps of one type. Views and samplers hold theirs
// and give them back when destroyed, which can be after the context goes.
// Scripts that make views every frame can have many waiting on Luau's GC, so
// a full heap adds another rather than refusing.
class D3D12DescriptorSlots : public RefCnt<D3D12DescriptorSlots>
{
public:
  D3D12DescriptorSlots(
      ID3D12Device* device,
      D3D12_DESCRIPTOR_HEAP_TYPE type,
      UINT heapSize)
      : m_device(device)
      , m_type(type)
      , m_heapSize(heapSize)
      , m_descriptorSize(device->GetDescriptorHandleIncrementSize(type))
  {}

  // Empty when another heap can't be made.
  D3D12DescriptorSlot take()
  {
    UINT index;
    if (!m_free.empty())
    {
      index = m_free.back();
      m_free.pop_back();
    }
    else
    {
      if (m_next == m_heapSize * static_cast<UINT>(m_starts.size()) &&
          !addHeap())
      {
        return {};
      }
      index = m_next++;
    }
    return D3D12DescriptorSlot(ref_rcp(this), index);
  }

  D3D12_CPU_DESCRIPTOR_HANDLE handle(UINT index) const
  {
    D3D12_CPU_DESCRIPTOR_HANDLE h = m_starts[index / m_heapSize];
    h.ptr += static_cast<SIZE_T>(index % m_heapSize) * m_descriptorSize;
    return h;
  }

  size_t heapCount() const { return m_heaps.size(); }

private:
  friend struct D3D12DescriptorSlot;

  bool addHeap()
  {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = m_type;
    desc.NumDescriptors = m_heapSize;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
    if (FAILED(m_device->CreateDescriptorHeap(
            &desc,
            IID_PPV_ARGS(heap.GetAddressOf()))))
    {
      return false;
    }
    m_starts.push_back(heap->GetCPUDescriptorHandleForHeapStart());
    m_heaps.push_back(std::move(heap));
    return true;
  }

  void giveBack(UINT index) { m_free.push_back(index); }

  Microsoft::WRL::ComPtr<ID3D12Device> m_device;
  D3D12_DESCRIPTOR_HEAP_TYPE m_type;
  UINT m_heapSize;
  UINT m_descriptorSize;
  std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> m_heaps;
  std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_starts;
  UINT m_next = 0;
  std::vector<UINT> m_free;
};

inline D3D12DescriptorSlot& D3D12DescriptorSlot::operator=(
    D3D12DescriptorSlot&& other) noexcept
{
  if (this != &other)
  {
    release();
    pool = std::move(other.pool);
    index = other.index;
    other.index = UINT_MAX;
  }
  return *this;
}

inline D3D12_CPU_DESCRIPTOR_HANDLE D3D12DescriptorSlot::handle() const
{
  return pool->handle(index);
}

inline void D3D12DescriptorSlot::release()
{
  if (pool != nullptr)
  {
    pool->giveBack(index);
    pool = nullptr;
    index = UINT_MAX;
  }
}

class ContextD3D12 : public Context
{
public:
  // Takes the D3D12 device and command queue. Ore shares the caller's
  // queue so that canvas-texture renders are visible to the host renderer
  // without cross-queue synchronization.
  static std::unique_ptr<ContextD3D12> Make(
      rcp<rive::gpu::D3D12ResourceManager> manager,
      ID3D12Device* device);

  ~ContextD3D12() override;

  rcp<Buffer> makeBuffer(const BufferDesc& desc) override;
  rcp<Texture> makeTexture(const TextureDesc& desc) override;
  rcp<TextureView> makeTextureViewImpl(const TextureViewDesc& desc) override;
  rcp<Sampler> makeSampler(const SamplerDesc& desc) override;
  rcp<ShaderModule> makeShaderModule(const ShaderModuleDesc& desc) override;
  rcp<BindGroupLayout> makeBindGroupLayout(
      const BindGroupLayoutDesc& desc) override;
  rcp<Pipeline> makePipeline(
      const PipelineDesc& desc,
      std::string* outError = nullptr) override;
  rcp<BindGroup> makeBindGroup(const BindGroupDesc& desc) override;

  std::unique_ptr<RenderPass> beginRenderPass(
      const RenderPassDesc& desc,
      std::string* outError = nullptr) override;

  void beginFrame(const FrameDescriptor&) override;
  void endFrame() override;
  void waitForGPU() override;

  // Frame numbers from the host, used by BufferD3D12 to version backings:
  // a backing bound in `currentFrameNumber` is reclaimable once
  // `safeFrameNumber` reaches it.
  uint64_t currentFrameNumber() const { return m_currentFrameNumber; }
  uint64_t safeFrameNumber() const { return m_safeFrameNumber; }

  // Lets tests check freed slots come back instead of the heaps growing.
  size_t d3d12CpuHeapCount() const;

  rcp<TextureView> wrapCanvasTexture(gpu::RenderCanvas* canvas) override;
  rcp<TextureView> wrapRenderTarget(gpu::RenderTarget* target) override;
  rcp<TextureView> wrapRiveTexture(
      gpu::Texture* gpuTex,
      uint32_t width,
      uint32_t height) override;

  ShaderTarget shaderTarget() const override { return ShaderTarget::hlsl; }

  ContextD3D12(const ContextD3D12&) = delete;
  ContextD3D12& operator=(const ContextD3D12&) = delete;

private:
  friend class RenderPassD3D12;
  friend class BindGroupD3D12;
  friend class TextureD3D12;
  friend class BufferD3D12;

  ContextD3D12(rcp<rive::gpu::GPUResourceManager> manager)
      : Context(std::move(manager))
  {}

  // D3D12 implementation helpers — defined in ore_context_d3d12.cpp.
  rcp<Buffer> d3d12MakeBuffer(const BufferDesc& desc);
  // Create + persistently map one UPLOAD-heap backing of exactly
  // `alignedSize` bytes. Shared by buffer creation and orphan-on-bound.
  bool d3d12AllocBufferBacking(
      UINT64 alignedSize,
      Microsoft::WRL::ComPtr<ID3D12Resource>& outResource,
      void** outMapped);
  rcp<Texture> d3d12MakeTexture(const TextureDesc& desc);
  rcp<TextureView> d3d12MakeTextureView(const TextureViewDesc& desc);
  rcp<Sampler> d3d12MakeSampler(const SamplerDesc& desc);
  rcp<ShaderModule> d3d12MakeShaderModule(const ShaderModuleDesc& desc);
  rcp<Pipeline> d3d12MakePipeline(
      const PipelineDesc& desc,
      std::string* outError);
  rcp<BindGroup> d3d12MakeBindGroup(const BindGroupDesc& desc);
  rcp<BindGroupLayout> d3d12MakeBindGroupLayout(
      const BindGroupLayoutDesc& desc);
  std::unique_ptr<RenderPass> d3d12BeginRenderPass(
      const RenderPassDesc& desc,
      std::string* outError);
  rcp<TextureView> d3d12WrapTarget(gpu::RenderTarget* target, bool canvas);
  rcp<TextureView> d3d12WrapRiveTexture(
      gpu::Texture* gpuTex,
      uint32_t w,
      uint32_t h);

  // Recording a copy directly would land on the host command list even while
  // it is closed between frames, so uploads stage here until one is live.
  struct D3D12PendingTextureUpload
  {
    rcp<Texture> texture;
    rcp<Buffer> staging;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
    UINT subresource;
    UINT dstX;
    UINT dstY;
    UINT dstZ;
  };
  void d3d12QueuePendingTextureUpload(D3D12PendingTextureUpload pending);
  void d3d12FlushPendingTextureUploads();

  Microsoft::WRL::ComPtr<ID3D12Device> m_d3dDevice;
  // Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_d3dQueue;
  //  Active command list for the current frame. Points at m_d3dOwnedCmdList
  //  in owned-CL mode and at the host-provided command list in external-CL
  //  mode. All recording code reads through this pointer, so the two modes
  //  share one code path.
  ID3D12GraphicsCommandList* m_d3dCmdList = nullptr;
  // Drained at the next beginFrame or beginRenderPass.
  std::vector<D3D12PendingTextureUpload> m_d3dPendingUploads;
  // resource-creation time.
  // Destroyed views and samplers give their slots back.
  rcp<D3D12DescriptorSlots> m_d3dCpuSrvSlots;
  rcp<D3D12DescriptorSlots> m_d3dCpuRtvSlots;
  rcp<D3D12DescriptorSlots> m_d3dCpuDsvSlots;
  rcp<D3D12DescriptorSlots> m_d3dCpuSamplerSlots;
  // Host targets are wrapped anew every frame, so they share one RTV slot,
  // and the null descriptors keep theirs. Held for the context's life.
  D3D12DescriptorSlot m_d3dTargetRtvSlot;
  D3D12DescriptorSlot m_d3dNullSrvSlot;
  D3D12DescriptorSlot m_d3dNullSamplerSlot;
  // Draws read their tables when they execute, so a full heap waits in its
  // pool until its frame is safe.
  struct D3D12GpuHeap
  {
    rcp<gpu::D3D12DescriptorHeapPool> pool;
    rcp<gpu::D3D12DescriptorHeap> heap;
    UINT size = 0;
    UINT allocated = 0;
  };
  static constexpr UINT kGpuSrvHeapSize = 4096;
  static constexpr UINT kGpuSamplerHeapSize = 512;
  D3D12GpuHeap m_d3dGpuSrvHeap;
  D3D12GpuHeap m_d3dGpuSamplerHeap;
  // Bumped each time the heaps are set on the list, which drops every table
  // set before.
  uint32_t m_d3dGpuHeapGeneration = 0;
  void d3d12SetGpuHeaps();
  // Host frame numbers captured at beginFrame for backing reclamation.
  uint64_t m_currentFrameNumber = 0;
  uint64_t m_safeFrameNumber = 0;
  // Called by RenderPass. Moves to a fresh heap when this one is full.
  UINT d3d12AllocGpuSlots(D3D12GpuHeap& gpuHeap, UINT count);
  // Sets lastError when another heap can't be made.
  bool d3d12TakeSlot(
      D3D12DescriptorSlots& slots,
      D3D12DescriptorSlot& out,
      const char* caller,
      const char* kind);
};

} // namespace rive::ore

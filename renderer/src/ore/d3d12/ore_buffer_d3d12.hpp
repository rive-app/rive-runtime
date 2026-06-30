#pragma once
#include "rive/renderer/ore/ore_buffer.hpp"
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>

namespace rive::ore
{
class ContextD3D12;

// A write after a bind orphans onto a fresh backing instead of racing the GPU
// still reading the bound one. Bindings resolve the live backing at apply time.
// Immutable or never rebound buffers stay at a single backing.
class BufferD3D12 : public LITE_RTTI_OVERRIDE(Buffer, BufferD3D12)
{
public:
    BufferD3D12(rcp<rive::gpu::GPUResourceManager> manager,
                uint32_t size,
                BufferUsage usage) :
        lite_rtti_override(std::move(manager), size, usage)
    {}
    ~BufferD3D12() override = default; // ComPtr released automatically
    void update(const void* data, uint32_t size, uint32_t offset) override;

    // GPU-VA of the live backing. Call at apply time, not creation time.
    D3D12_GPU_VIRTUAL_ADDRESS currentGpuVA() const
    {
        return m_d3dBuffer->GetGPUVirtualAddress();
    }

    // Stamp the live backing with this frame so a later update() orphans
    // instead of overwriting memory the GPU may still read.
    void markBound();

private:
    friend class ContextD3D12;
    friend class RenderPassD3D12;
    friend class TextureD3D12; // for staging buffer access during texture
                               // uploads

    // Move to a fresh backing, copying current contents so a partial update
    // keeps untouched bytes. The pending write's range lets a full-buffer
    // update skip the copy. Returns false if a fresh backing could not be
    // allocated, in which case the current backing is kept.
    bool acquireFreshBacking(uint32_t writeOffset, uint32_t writeSize);

    // Live backing in an UPLOAD heap, persistently mapped so the GPU reads it.
    Microsoft::WRL::ComPtr<ID3D12Resource> m_d3dBuffer;
    void* m_d3dMappedPtr = nullptr;

    struct Backing
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        void* mapped = nullptr;
        uint64_t serial = 0; // frame it was last bound in
    };
    std::vector<Backing> m_retired; // rotated-out backings awaiting GPU retire
    uint64_t m_currentSerial = 0;   // frame the live backing was last bound in
    bool m_boundSinceUpdate = false;
    UINT64 m_allocatedSize = 0; // aligned size for fresh backings
    ContextD3D12* m_ctx = nullptr;
};
} // namespace rive::ore

#pragma once
#include "rive/renderer/ore/ore_buffer.hpp"
#include <d3d12.h>
#include <wrl/client.h>

namespace rive::ore
{
class ContextD3D12;

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

private:
    friend class ContextD3D12;
    friend class RenderPassD3D12;
    friend class TextureD3D12; // for staging buffer access during texture
                               // uploads
    // UPLOAD heap — persistently mapped; GPU reads directly.
    Microsoft::WRL::ComPtr<ID3D12Resource> m_d3dBuffer;
    void* m_d3dMappedPtr = nullptr;
};
} // namespace rive::ore

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
    BufferD3D12(uint32_t size, BufferUsage usage) :
        lite_rtti_override(size, usage)
    {}
    ~BufferD3D12() override = default; // ComPtr released automatically
    void update(const void* data, uint32_t size, uint32_t offset) override;
    // D3D12 defers the entire CPU object deletion until after GPU fence.
    void onRefCntReachedZero() const override;

private:
    friend class ContextD3D12;
    friend class RenderPassD3D12;
    // UPLOAD heap — persistently mapped; GPU reads directly.
    Microsoft::WRL::ComPtr<ID3D12Resource> m_d3dBuffer;
    void* m_d3dMappedPtr = nullptr;
    // Back-ref so onRefCntReachedZero() can route deletion through
    // ContextD3D12::d3dDeferDestroy() in external-CL mode. Weak ref.
    ContextD3D12* m_d3dOreContext = nullptr;
};
} // namespace rive::ore

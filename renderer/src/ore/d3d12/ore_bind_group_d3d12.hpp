#pragma once
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include <d3d12.h>
#include <cstdint>

namespace rive::ore
{
class ContextD3D12;

class BindGroupD3D12 : public LITE_RTTI_OVERRIDE(BindGroup, BindGroupD3D12)
{
public:
    BindGroupD3D12() = default;
    ~BindGroupD3D12() override = default;

private:
    friend class ContextD3D12;
    friend class RenderPassD3D12;
    // UBO (CBV) slots — GPU virtual addresses and sizes indexed by native slot.
    D3D12_GPU_VIRTUAL_ADDRESS m_d3dUBOAddresses[8] = {};
    uint32_t m_d3dUBOSizes[8] = {};
    uint8_t m_d3dUBOSlotMask = 0;
    uint8_t m_d3dUBODynamicOffsetMask = 0;
    // SRV (texture) slots — CPU descriptor handles and texture back-refs.
    D3D12_CPU_DESCRIPTOR_HANDLE m_d3dSrvHandles[8] = {};
    Texture* m_d3dSrvTextures[8] =
        {}; // Weak refs; kept alive by m_retainedViews.
    uint8_t m_d3dSrvSlotMask = 0;
    // Sampler slots — CPU descriptor handles.
    D3D12_CPU_DESCRIPTOR_HANDLE m_d3dSamplerHandles[8] = {};
    uint8_t m_d3dSamplerSlotMask = 0;
};
} // namespace rive::ore

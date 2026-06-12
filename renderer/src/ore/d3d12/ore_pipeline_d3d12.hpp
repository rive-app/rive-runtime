#pragma once
#include "rive/renderer/ore/ore_pipeline.hpp"
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>

namespace rive::ore
{
class ContextD3D12;

// Per-group descriptor-table shape for D3D12 root signatures.
// Populated by ContextD3D12::makeBindGroupLayout() via
// tallyGroupFromLayoutDesc() and copied into PipelineD3D12::m_d3dGroupParams by
// makePipeline().
struct D3D12GroupRootParams
{
    uint8_t cbvCount = 0;
    uint8_t srvCount = 0;
    uint8_t uavCount = 0;
    uint8_t samplerCount = 0;
    uint8_t cbvBaseGlobalSlot = 0;
    uint8_t srvBaseGlobalSlot = 0;
    uint8_t uavBaseGlobalSlot = 0;
    uint8_t samplerBaseGlobalSlot = 0;
    // Root-parameter indices assigned at PSO build time (-1 = absent).
    int8_t cbvSrvUavRootParamIdx = -1;
    int8_t samplerRootParamIdx = -1;
};

class PipelineD3D12 : public LITE_RTTI_OVERRIDE(Pipeline, PipelineD3D12)
{
public:
    PipelineD3D12(rcp<rive::gpu::GPUResourceManager> manager,
                  const PipelineDesc& desc) :
        lite_rtti_override(std::move(manager), desc)
    {}
    ~PipelineD3D12() override = default;

private:
    friend class ContextD3D12;
    friend class RenderPassD3D12;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_d3dPSO;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_d3dRootSigOwned;
    D3D12_PRIMITIVE_TOPOLOGY m_d3dTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    // Mirrored from BindGroupLayouts at makePipeline() time.
    D3D12GroupRootParams m_d3dGroupParams[kMaxBindGroups];
    // Per-VB stride for setVertexBuffer().
    uint32_t m_d3dVertexStrides[8] = {};
};
} // namespace rive::ore

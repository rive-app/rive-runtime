#pragma once
#include "rive/renderer/ore/ore_bind_group_layout.hpp"
#include "ore_pipeline_d3d12.hpp" // D3D12GroupRootParams

namespace rive::ore
{
class ContextD3D12;

class BindGroupLayoutD3D12
    : public LITE_RTTI_OVERRIDE(BindGroupLayout, BindGroupLayoutD3D12)
{
public:
    BindGroupLayoutD3D12(rcp<rive::gpu::GPUResourceManager> manager) :
        lite_rtti_override(std::move(manager))
    {}
    ~BindGroupLayoutD3D12() override = default;

    // Accessor for use by D3D12 root-signature builder.
    const D3D12GroupRootParams& d3dGroupParams() const
    {
        return m_d3dGroupParams;
    }

private:
    friend class ContextD3D12;
    D3D12GroupRootParams m_d3dGroupParams;
};
} // namespace rive::ore

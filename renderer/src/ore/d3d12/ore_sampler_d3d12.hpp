#pragma once
#include "rive/renderer/ore/ore_sampler.hpp"
#include <d3d12.h>

namespace rive::ore
{
class ContextD3D12;

class SamplerD3D12 : public LITE_RTTI_OVERRIDE(Sampler, SamplerD3D12)
{
public:
    SamplerD3D12() = default;
    ~SamplerD3D12() override = default;
    // Defers the entire CPU object deletion until after GPU fence.
    void onRefCntReachedZero() const override;

private:
    friend class ContextD3D12;
    D3D12_CPU_DESCRIPTOR_HANDLE m_d3dSamplerHandle = {};
    // Back-ref so onRefCntReachedZero() can route deletion through
    // ContextD3D12::d3dDeferDestroy() in external-CL mode. Weak ref.
    ContextD3D12* m_d3dOreContext = nullptr;
};
} // namespace rive::ore

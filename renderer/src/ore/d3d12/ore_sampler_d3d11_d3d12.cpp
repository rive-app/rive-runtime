/*
 * Copyright 2025 Rive
 */

// Combined D3D11 + D3D12 Sampler implementation for Windows.
// Compiled when both ORE_BACKEND_D3D11 and ORE_BACKEND_D3D12 are active.

#if defined(ORE_BACKEND_D3D11) && defined(ORE_BACKEND_D3D12)

#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_context.hpp"

namespace rive::ore
{

void Sampler::onRefCntReachedZero() const
{
    // m_d3dOreContext is only set on D3D12-backed samplers (see
    // d3d12MakeSampler). D3D11-backed samplers keep the immediate-delete
    // path unchanged.
    Context* ctx = m_d3dOreContext;
    auto destroy = [p = const_cast<Sampler*>(this)]() { delete p; };
    if (ctx != nullptr)
        ctx->d3dDeferDestroy(std::move(destroy));
    else
        destroy();
}

} // namespace rive::ore

#endif // ORE_BACKEND_D3D11 && ORE_BACKEND_D3D12

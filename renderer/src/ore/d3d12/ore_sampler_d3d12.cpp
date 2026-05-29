/*
 * Copyright 2025 Rive
 */

#include "ore_sampler_d3d12.hpp"
#include "rive/renderer/ore/ore_context_d3d12.hpp"

namespace rive::ore
{

// --- Public method definitions (D3D12-only builds) ---
// When both D3D11 and D3D12 are compiled, the combined
// ore_context_d3d11_d3d12.cpp file provides these methods with dispatch.
#if defined(ORE_BACKEND_D3D12)

void SamplerD3D12::onRefCntReachedZero() const
{
    ContextD3D12* ctx = m_d3dOreContext;
    auto destroy = [p = const_cast<SamplerD3D12*>(this)]() { delete p; };
    if (ctx != nullptr)
        ctx->d3dDeferDestroy(std::move(destroy));
    else
        destroy();
}

#endif // D3D12-only

} // namespace rive::ore

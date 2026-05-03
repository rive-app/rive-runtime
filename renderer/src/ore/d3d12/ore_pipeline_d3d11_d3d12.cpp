/*
 * Copyright 2025 Rive
 */

// Combined D3D11 + D3D12 Pipeline implementation for Windows.
// Compiled when both ORE_BACKEND_D3D11 and ORE_BACKEND_D3D12 are active.

#if defined(ORE_BACKEND_D3D11) && defined(ORE_BACKEND_D3D12)

#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"
#include "rive/renderer/ore/ore_context.hpp"

namespace rive::ore
{

void Pipeline::onRefCntReachedZero() const
{
    // m_d3dOreContext is only set on D3D12-backed pipelines (see
    // d3d12MakePipeline). D3D11-backed pipelines keep the immediate-delete
    // path unchanged.
    Context* ctx = m_d3dOreContext;
    auto destroy = [p = const_cast<Pipeline*>(this)]() { delete p; };
    if (ctx != nullptr)
        ctx->d3dDeferDestroy(std::move(destroy));
    else
        destroy();
}

void BindGroupLayout::onRefCntReachedZero() const
{
    // m_context is set for D3D12 layouts (deferred destroy); D3D11 layouts
    // have m_context == nullptr and use the immediate-delete path.
    Context* ctx = m_context;
    auto destroy = [p = const_cast<BindGroupLayout*>(this)]() { delete p; };
    if (ctx != nullptr)
        ctx->d3dDeferDestroy(std::move(destroy));
    else
        destroy();
}

} // namespace rive::ore

#endif // ORE_BACKEND_D3D11 && ORE_BACKEND_D3D12

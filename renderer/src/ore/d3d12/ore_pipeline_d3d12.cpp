/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"
#include "rive/renderer/ore/ore_context.hpp"

namespace rive::ore
{

// --- Public method definitions (D3D12-only builds) ---
// When both D3D11 and D3D12 are compiled, the combined
// ore_context_d3d11_d3d12.cpp file provides these methods with dispatch.
#if defined(ORE_BACKEND_D3D12) && !defined(ORE_BACKEND_D3D11)

void Pipeline::onRefCntReachedZero() const
{
    // Capture the context before `delete this` invalidates `m_d3dOreContext`.
    // In owned-CL mode d3dDeferDestroy runs the closure immediately, matching
    // prior behavior. In external-CL mode it queues the closure until the
    // next beginFrame() drains it, so the ComPtr release (and the underlying
    // D3D12 resource destroy) can't happen while the host's command list
    // still references the pipeline state object.
    Context* ctx = m_d3dOreContext;
    auto destroy = [p = const_cast<Pipeline*>(this)]() { delete p; };
    if (ctx != nullptr)
        ctx->d3dDeferDestroy(std::move(destroy));
    else
        destroy();
}

void BindGroupLayout::onRefCntReachedZero() const
{
    // Layout has no GPU handle on D3D12 (root sig owns the descriptor
    // table shape; it's per-pipeline). Just delete.
    Context* ctx = m_context;
    auto destroy = [p = const_cast<BindGroupLayout*>(this)]() { delete p; };
    if (ctx != nullptr)
        ctx->d3dDeferDestroy(std::move(destroy));
    else
        destroy();
}

#endif // D3D12-only

} // namespace rive::ore

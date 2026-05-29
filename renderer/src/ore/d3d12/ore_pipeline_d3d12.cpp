/*
 * Copyright 2025 Rive
 */

#include "ore_pipeline_d3d12.hpp"
#include "ore_bind_group_layout_d3d12.hpp"
#include "rive/renderer/ore/ore_context_d3d12.hpp"

namespace rive::ore
{

// --- Public method definitions (D3D12-only builds) ---
// When both D3D11 and D3D12 are compiled, the combined
// ore_context_d3d11_d3d12.cpp file provides these methods with dispatch.
#if defined(ORE_BACKEND_D3D12)

void PipelineD3D12::onRefCntReachedZero() const
{
    // Capture the context before `delete this` invalidates `m_d3dOreContext`.
    // In owned-CL mode d3dDeferDestroy runs the closure immediately, matching
    // prior behavior. In external-CL mode it queues the closure until the
    // next beginFrame() drains it, so the ComPtr release (and the underlying
    // D3D12 resource destroy) can't happen while the host's command list
    // still references the pipeline state object.
    ContextD3D12* ctx = m_d3dOreContext;
    auto destroy = [p = const_cast<PipelineD3D12*>(this)]() { delete p; };
    if (ctx != nullptr)
        ctx->d3dDeferDestroy(std::move(destroy));
    else
        destroy();
}

void BindGroupLayoutD3D12::onRefCntReachedZero() const
{
    // Layout has no GPU handle on D3D12 (root sig owns the descriptor
    // table shape; it's per-pipeline). Just delete.
    ContextD3D12* ctx = static_cast<ContextD3D12*>(m_context);
    auto destroy = [p = const_cast<BindGroupLayoutD3D12*>(this)]() {
        delete p;
    };
    if (ctx != nullptr)
        ctx->d3dDeferDestroy(std::move(destroy));
    else
        destroy();
}

#endif // D3D12-only

} // namespace rive::ore

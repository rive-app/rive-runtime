/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_bind_group.hpp"

namespace rive::ore
{

// --- Public method definitions (D3D12-only builds) ---
// When both D3D11 and D3D12 are compiled, the combined
// ore_context_d3d11_d3d12.cpp file provides these methods with dispatch.
#if defined(ORE_BACKEND_D3D12) && !defined(ORE_BACKEND_D3D11)

void BindGroup::onRefCntReachedZero() const
{
    // Deferred destruction is handled at the Lua GC level via
    // Context::deferBindGroupDestroy(), which keeps the rcp<> alive
    // until after endFrame(). By the time refcount reaches zero here,
    // the GPU is guaranteed to be done.
    delete this;
}

#endif // D3D12-only

} // namespace rive::ore

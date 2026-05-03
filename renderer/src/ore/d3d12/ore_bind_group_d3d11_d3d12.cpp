/*
 * Copyright 2025 Rive
 */

// Combined D3D11 + D3D12 BindGroup implementation for Windows.
// Compiled when both ORE_BACKEND_D3D11 and ORE_BACKEND_D3D12 are active.

#if defined(ORE_BACKEND_D3D11) && defined(ORE_BACKEND_D3D12)

#include "rive/renderer/ore/ore_bind_group.hpp"

namespace rive::ore
{

void BindGroup::onRefCntReachedZero() const
{
    // Deferred destruction is handled at the Lua GC level via
    // Context::deferBindGroupDestroy(), which keeps the rcp<> alive
    // until after endFrame(). By the time refcount reaches zero here,
    // the GPU is guaranteed to be done.
    delete this;
}

} // namespace rive::ore

#endif // ORE_BACKEND_D3D11 && ORE_BACKEND_D3D12

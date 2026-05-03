/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_bind_group.hpp"

namespace rive::ore
{

#if defined(ORE_BACKEND_D3D11) && !defined(ORE_BACKEND_D3D12)

void BindGroup::onRefCntReachedZero() const
{
    // Deferred destruction is handled at the Lua GC level via
    // Context::deferBindGroupDestroy(), which keeps the rcp<> alive
    // until after endFrame(). By the time refcount reaches zero here,
    // the GPU is guaranteed to be done.
    delete this;
}

#endif // ORE_BACKEND_D3D11 && !ORE_BACKEND_D3D12

} // namespace rive::ore

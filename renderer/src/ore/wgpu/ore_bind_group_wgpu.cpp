/*
 * Copyright 2025 Rive
 */

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

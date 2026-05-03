/*
 * Copyright 2025 Rive
 */

// Combined Metal + GL BindGroup implementation for macOS.
// Compiled when both ORE_BACKEND_METAL and ORE_BACKEND_GL are active.

#if defined(ORE_BACKEND_METAL) && defined(ORE_BACKEND_GL)

#include "rive/renderer/gl/load_gles_extensions.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_context.hpp"

#import <Metal/Metal.h>

namespace rive::ore
{

void BindGroup::onRefCntReachedZero() const
{
    // Deferred destruction is handled at the Lua GC level via
    // Context::deferBindGroupDestroy(). By the time refcount reaches
    // zero here, the GPU is guaranteed to be done.
    delete this;
}

} // namespace rive::ore

#endif // ORE_BACKEND_METAL && ORE_BACKEND_GL

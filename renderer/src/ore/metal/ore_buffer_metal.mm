/*
 * Copyright 2025 Rive
 */

#include "ore_buffer_metal.hpp"
#include "rive/renderer/ore/ore_context_metal.hpp"
#include "rive/rive_types.hpp"

#import <Metal/Metal.h>

namespace rive::ore
{

void BufferMetal::markBound()
{
    m_pool[m_currentIndex].serial = m_ctx->currentSerial();
    m_boundSinceUpdate = true;
}

bool BufferMetal::acquireFreshBacking(uint32_t writeOffset, uint32_t writeSize)
{
    id<MTLBuffer> old = m_pool[m_currentIndex].mtl;
    const uint64_t completed = m_ctx->completedSerial();

    // Reuse a backing the GPU has finished with, else allocate one.
    size_t fresh = m_pool.size();
    for (size_t i = 0; i < m_pool.size(); ++i)
    {
        if (i != m_currentIndex && m_pool[i].serial <= completed)
        {
            fresh = i;
            break;
        }
    }
    if (fresh == m_pool.size())
    {
        id<MTLBuffer> mtl =
            [m_ctx->device() newBufferWithLength:m_size
                                         options:MTLResourceStorageModeShared];
        if (mtl == nil)
        {
            // Keep the current backing. Report it like the D3D12 path so the
            // degraded (racy) fallback is diagnosable.
            m_ctx->setLastError("ore: Metal buffer backing allocation failed; "
                                "reusing in flight backing for this update");
            return false;
        }
        if (!m_label.empty())
            mtl.label = @(m_label.c_str());
        m_pool.push_back({mtl, 0});
    }

    m_currentIndex = fresh;
    // Carry contents forward so a partial update keeps untouched bytes. Skip
    // when this update covers the whole buffer.
    if (!(writeOffset == 0 && writeSize == m_size))
        memcpy([m_pool[m_currentIndex].mtl contents], [old contents], m_size);
    return true;
}

void BufferMetal::update(const void* data, uint32_t size, uint32_t offset)
{
    assert(offset + size <= m_size);
    if (m_boundSinceUpdate)
    {
        // On allocation failure keep the current backing and retry next update.
        if (acquireFreshBacking(offset, size))
            m_boundSinceUpdate = false;
    }
    memcpy(static_cast<uint8_t*>([m_pool[m_currentIndex].mtl contents]) +
               offset,
           data,
           size);
}

} // namespace rive::ore

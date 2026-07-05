/*
 * Copyright 2025 Rive
 */

#include "ore_buffer_d3d12.hpp"
#include "rive/renderer/ore/ore_context_d3d12.hpp"
#include "rive/rive_types.hpp"

#include <d3d12.h>
#include <cassert>
#include <cstring>

namespace rive::ore
{

// Guarded by ORE_BACKEND_D3D12. The Windows build compiles both the d3d11 and
// d3d12 dirs with both backend macros defined; BufferD3D11 and BufferD3D12 are
// distinct classes under their own guards and the context factory selects the
// concrete type at runtime, so there is no shared dispatch file.
#if defined(ORE_BACKEND_D3D12)

void BufferD3D12::markBound()
{
    m_currentSerial = m_ctx->currentFrameNumber();
    m_boundSinceUpdate = true;
}

bool BufferD3D12::acquireFreshBacking(uint32_t writeOffset, uint32_t writeSize)
{
    // Retire the current backing since an in flight bind may still read it.
    void* oldMapped = m_d3dMappedPtr;
    m_retired.push_back(
        {std::move(m_d3dBuffer), m_d3dMappedPtr, m_currentSerial});

    // Reuse a retired backing the GPU has finished with, else allocate one.
    // Require the bind to predate the current frame, not just safeFrameNumber,
    // so a host that ever reports safe == current cannot hand back an in flight
    // backing.
    const uint64_t safe = m_ctx->safeFrameNumber();
    const uint64_t current = m_ctx->currentFrameNumber();
    for (auto it = m_retired.begin(); it != m_retired.end(); ++it)
    {
        if (it->serial <= safe && it->serial < current)
        {
            m_d3dBuffer = std::move(it->resource);
            m_d3dMappedPtr = it->mapped;
            m_retired.erase(it);
            break;
        }
    }
    if (m_d3dBuffer == nullptr &&
        !m_ctx->d3d12AllocBufferBacking(m_allocatedSize,
                                        m_d3dBuffer,
                                        &m_d3dMappedPtr))
    {
        // Allocation failed. Keep the backing we just retired so we do not
        // write through a null map. This reraces this one update but the data
        // is already there, and it avoids a crash.
        m_ctx->setLastError("ore: D3D12 buffer backing allocation failed; "
                            "reusing in flight backing for this update");
        Backing& old = m_retired.back();
        m_d3dBuffer = std::move(old.resource);
        m_d3dMappedPtr = old.mapped;
        m_retired.pop_back();
        return false; // Kept the current backing; retry orphaning next update.
    }

    m_currentSerial = 0;
    // Carry contents forward so a partial update keeps untouched bytes. Skip
    // when this update covers the whole buffer, and skip the self copy when the
    // reused backing is the one we just retired.
    if (!(writeOffset == 0 && writeSize == m_size) &&
        m_d3dMappedPtr != oldMapped)
        memcpy(m_d3dMappedPtr, oldMapped, m_size);
    return true;
}

void BufferD3D12::update(const void* data, uint32_t size, uint32_t offset)
{
    assert(offset + size <= m_size);
    assert(m_d3dBuffer != nullptr);
    assert(m_d3dMappedPtr != nullptr);

    if (m_boundSinceUpdate)
    {
        // On allocation failure keep the current backing and retry next update.
        if (acquireFreshBacking(offset, size))
            m_boundSinceUpdate = false;
    }
    // UPLOAD heap buffer is persistently mapped so write directly.
    memcpy(static_cast<uint8_t*>(m_d3dMappedPtr) + offset, data, size);
}

#endif // D3D12-only

} // namespace rive::ore

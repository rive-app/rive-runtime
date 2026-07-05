/*
 * Copyright 2025 Rive
 */

#include "ore_buffer_wgpu.hpp"
#include "rive/renderer/ore/ore_context_wgpu.hpp"
#include "rive/rive_types.hpp"

#include <cstring>

namespace rive::ore
{

void BufferWGPU::markBound()
{
    m_pool[m_currentIndex].frameStamp = m_ctx->currentFrameSerial();
    m_boundSinceUpdate = true;
}

bool BufferWGPU::acquireFreshBacking()
{
    const uint64_t serial = m_ctx->currentFrameSerial();

    // Reuse a backing from a prior frame, else allocate one. The current
    // backing is never a candidate, and neither is one bound this frame. This
    // relies on exactly one queue submit per frame serial: the rewrite of a
    // prior-frame backing is ordered after that frame's submit, so its reads
    // finish first. If Ore ever shares the host encoder across frames with no
    // submit between, stamp on submit instead of on beginFrame.
    size_t fresh = m_pool.size();
    for (size_t i = 0; i < m_pool.size(); ++i)
    {
        if (i != m_currentIndex && m_pool[i].frameStamp != serial)
        {
            fresh = i;
            break;
        }
    }
    if (fresh == m_pool.size())
    {
        wgpu::BufferDescriptor wDesc{};
        wDesc.size = m_size;
        wDesc.usage = m_wgpuUsage;
        wDesc.mappedAtCreation = false;
        wgpu::Buffer buffer = m_wgpuDevice.CreateBuffer(&wDesc);
        if (buffer == nullptr)
        {
            // Keep the current backing. Report it like the D3D12 path so the
            // degraded (racy) fallback is diagnosable.
            m_ctx->setLastError("ore: WGPU buffer backing allocation failed; "
                                "reusing in flight backing for this update");
            return false;
        }
        m_pool.push_back({std::move(buffer), 0});
    }
    m_currentIndex = fresh;
    return true;
}

void BufferWGPU::update(const void* data, uint32_t size, uint32_t offset)
{
    assert(offset + size <= m_size);
    bool orphaned = false;
    if (m_boundSinceUpdate)
    {
        // On allocation failure keep the current backing and retry next update.
        if (acquireFreshBacking())
        {
            m_boundSinceUpdate = false;
            orphaned = true;
        }
    }
    // Allocate the shadow on first write so a never updated buffer pays
    // nothing. It tracks writes from here so an orphan can rewrite the whole
    // backing.
    if (m_shadow.empty())
        m_shadow.resize(m_size, 0);
    memcpy(m_shadow.data() + offset, data, size);
    if (orphaned)
        m_wgpuQueue.WriteBuffer(current(), 0, m_shadow.data(), m_size);
    else
        m_wgpuQueue.WriteBuffer(current(), offset, data, size);
}

} // namespace rive::ore

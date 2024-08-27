/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/renderer/gpu.hpp"

namespace rive::gpu
{
// API-agnostic implementation of an abstract buffer ring. We use rings to ensure the GPU can render
// one frame in parallel while the CPU prepares the next frame.
//
// Calling mapBuffer() maps the next buffer in the ring.
//
// Calling unmapAndSubmitBuffer() submits the currently-mapped buffer for GPU rendering, in whatever
// way that is meaningful for the PLSRenderContext implementation.
//
// This class is meant to only be used through BufferRing<>.
class BufferRing
{
public:
    BufferRing(size_t capacityInBytes) : m_capacityInBytes(capacityInBytes) {}
    virtual ~BufferRing() {}

    size_t capacityInBytes() const { return m_capacityInBytes; }
    bool isMapped() const { return m_mapSizeInBytes != 0; }

    // Maps the next buffer in the ring.
    void* mapBuffer(size_t mapSizeInBytes)
    {
        assert(!isMapped());
        assert(mapSizeInBytes > 0);
        assert(mapSizeInBytes <= m_capacityInBytes);
        m_submittedBufferIdx = (m_submittedBufferIdx + 1) % kBufferRingSize;
        m_mapSizeInBytes = mapSizeInBytes;
        return onMapBuffer(m_submittedBufferIdx, m_mapSizeInBytes);
    }

    // Submits the currently-mapped buffer for GPU rendering, in whatever way that is meaningful for
    // the PLSRenderContext implementation.
    void unmapAndSubmitBuffer()
    {
        assert(isMapped());
        onUnmapAndSubmitBuffer(m_submittedBufferIdx, m_mapSizeInBytes);
        m_mapSizeInBytes = 0;
    }

protected:
    int submittedBufferIdx() const
    {
        assert(!isMapped());
        return m_submittedBufferIdx;
    }

    virtual void* onMapBuffer(int bufferIdx, size_t mapSizeInBytes) = 0;
    virtual void onUnmapAndSubmitBuffer(int bufferIdx, size_t mapSizeInBytes) = 0;

    uint8_t* shadowBuffer() const
    {
        if (m_shadowBuffer == nullptr && m_capacityInBytes > 0)
        {
            m_shadowBuffer.reset(new uint8_t[m_capacityInBytes]);
        }
        return m_shadowBuffer.get();
    }

private:
    size_t m_capacityInBytes;
    size_t m_mapSizeInBytes = 0;
    int m_submittedBufferIdx = 0;

    // Lazy-allocated CPU buffer for when buffer mapping isn't supported by the API.
    mutable std::unique_ptr<uint8_t[]> m_shadowBuffer;
};

// BufferRing that resides solely in CPU memory, and therefore doesn't require a ring.
class HeapBufferRing : public BufferRing
{
public:
    HeapBufferRing(size_t capacityInBytes) : BufferRing(capacityInBytes) {}

    uint8_t* contents() const { return shadowBuffer(); }

protected:
    void* onMapBuffer(int bufferIdx, size_t mapSizeInBytes) override { return shadowBuffer(); }
    void onUnmapAndSubmitBuffer(int bufferIdx, size_t mapSizeInBytes) override {}
};
} // namespace rive::gpu

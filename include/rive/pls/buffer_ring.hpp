/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/pls/pls.hpp"

namespace rive::pls
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
    BufferRing(size_t capacity, size_t itemSizeInBytes) :
        m_capacity(capacity), m_itemSizeInBytes(itemSizeInBytes)
    {}
    virtual ~BufferRing() {}

    size_t capacity() const { return m_capacity; }
    size_t itemSizeInBytes() const { return m_itemSizeInBytes; }
    size_t totalSizeInBytes() const { return m_capacity * m_itemSizeInBytes * kBufferRingSize; }

    // Maps the next buffer in the ring.
    void* mapBuffer()
    {
        assert(!m_mapped);
        RIVE_DEBUG_CODE(m_mapped = true;)
        m_submittedBufferIdx = (m_submittedBufferIdx + 1) % kBufferRingSize;
        return onMapBuffer(m_submittedBufferIdx);
    }

    // Submits the currently-mapped buffer for GPU rendering, in whatever way that is meaningful for
    // the PLSRenderContext implementation.
    void unmapAndSubmitBuffer(size_t bytesWritten)
    {
        assert(m_mapped);
        RIVE_DEBUG_CODE(m_mapped = false;)
        onUnmapAndSubmitBuffer(m_submittedBufferIdx, bytesWritten);
    }

protected:
    int submittedBufferIdx() const
    {
        assert(!m_mapped);
        return m_submittedBufferIdx;
    }

    virtual void* onMapBuffer(int bufferIdx) = 0;
    virtual void onUnmapAndSubmitBuffer(int bufferIdx, size_t bytesWritten) = 0;

private:
    size_t m_capacity;
    size_t m_itemSizeInBytes;
    int m_submittedBufferIdx = 0;
    RIVE_DEBUG_CODE(bool m_mapped = false;)
};

// Buffer ring implementation for a GPU resource that doesn't support mapping. Mapping is emulated
// via shadow buffer on the CPU.
class BufferRingShadowImpl : public BufferRing
{
public:
    BufferRingShadowImpl(size_t capacity, size_t itemSizeInBytes) :
        BufferRing(capacity, itemSizeInBytes)
    {}
    ~BufferRingShadowImpl() override { free(m_shadowBuffer); }

    void* onMapBuffer(int bufferIdx) final
    {
        if (!m_shadowBuffer)
        {
            m_shadowBuffer = malloc(capacity() * itemSizeInBytes());
        }
        return m_shadowBuffer;
    }

    const void* shadowBuffer() const { return m_shadowBuffer; }

private:
    void* m_shadowBuffer = nullptr;
};

// BufferRingShadowImpl that resides solely in CPU memory, and therefore doesn't require a ring.
class HeapBufferRing : public BufferRing
{
public:
    HeapBufferRing(size_t capacity, size_t itemSizeInBytes) :
        BufferRing(capacity, itemSizeInBytes), m_contents(new uint8_t[capacity * itemSizeInBytes])
    {}

    const void* contents() const { return m_contents.get(); }
    void* contents() { return m_contents.get(); }

protected:
    void* onMapBuffer(int bufferIdx) override { return contents(); }
    void onUnmapAndSubmitBuffer(int bufferIdx, size_t bytesWritten) override {}

private:
    std::unique_ptr<uint8_t[]> m_contents;
};
} // namespace rive::pls

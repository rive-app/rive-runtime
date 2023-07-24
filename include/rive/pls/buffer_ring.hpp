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
class BufferRingImpl
{
public:
    BufferRingImpl(size_t capacity, size_t itemSizeInBytes) :
        m_capacity(capacity), m_itemSizeInBytes(itemSizeInBytes)
    {}
    virtual ~BufferRingImpl() {}

    size_t capacity() const { return m_capacity; }
    size_t itemSizeInBytes() const { return m_itemSizeInBytes; }

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
class BufferRingShadowImpl : public BufferRingImpl
{
public:
    BufferRingShadowImpl(size_t capacity, size_t itemSizeInBytes) :
        BufferRingImpl(capacity, itemSizeInBytes)
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

// BufferRingShadowImpl whose backing store is a 2D texture.
class TexelBufferRing : public BufferRingShadowImpl
{
public:
    enum class Format
    {
        rgba8,
        rgba32f,
        rgba32ui,
    };

    constexpr static size_t BytesPerPixel(Format format)
    {
        switch (format)
        {
            case Format::rgba8:
                return 4;
            case Format::rgba32f:
            case Format::rgba32ui:
                return 4 * 4;
        }
        RIVE_UNREACHABLE();
    }

    enum class Filter
    {
        nearest,
        linear,
    };

    TexelBufferRing(Format format, size_t widthInItems, size_t height, size_t texelsPerItem) :
        BufferRingShadowImpl(height * widthInItems, texelsPerItem * BytesPerPixel(format)),
        m_format(format),
        m_widthInItems(widthInItems),
        m_height(height),
        m_texelsPerItem(texelsPerItem)
    {}

    size_t widthInItems() const { return m_widthInItems; }
    size_t widthInTexels() const { return m_widthInItems * m_texelsPerItem; }
    size_t height() const { return m_height; }

protected:
    void onUnmapAndSubmitBuffer(int bufferIdx, size_t bytesWritten) final
    {
        size_t texelsWritten = bytesWritten / BytesPerPixel(m_format);
        assert(texelsWritten * BytesPerPixel(m_format) == bytesWritten);
        size_t updateWidthInTexels = std::min(texelsWritten, widthInTexels());
        size_t updateHeight = (texelsWritten + widthInTexels() - 1) / widthInTexels();
        submitTexels(bufferIdx, updateWidthInTexels, updateHeight);
    }

    virtual void submitTexels(int textureIdx, size_t updateWidthInTexels, size_t updateHeight) = 0;

    const Format m_format;
    const size_t m_widthInItems;
    const size_t m_height;
    const size_t m_texelsPerItem;
};

// Buffer ring implementation for buffers that only exist on the CPU.
class CPUOnlyBufferRing : public BufferRingImpl
{
public:
    CPUOnlyBufferRing(size_t capacity, size_t itemSizeInBytes) :
        BufferRingImpl(capacity, itemSizeInBytes)
    {
        for (int i = 0; i < kBufferRingSize; ++i)
            m_cpuBuffers[i].reset(new char[capacity * itemSizeInBytes]);
    }

    const void* submittedata() const { return m_cpuBuffers[submittedBufferIdx()].get(); }

protected:
    void* onMapBuffer(int bufferIdx) override { return m_cpuBuffers[bufferIdx].get(); }
    void onUnmapAndSubmitBuffer(int bufferIdx, size_t bytesWritten) override {}

private:
    std::unique_ptr<char[]> m_cpuBuffers[kBufferRingSize];
};

// Wrapper for an abstract BufferRingImpl that supports mapping buffers, writing an array of items
// of the same type, and submitting for rendering.
//
// Intended usage pattern:
//
//  * Call ensureMapped() to map the next buffer in the ring.
//  * push() all items for rendering.
//  * Call submit() to unmap and submit the currently-mapped buffer for rendering, in whatever way
//    that is meaningful for the PLSRenderContext implementation.
//
template <typename T> class BufferRing
{
public:
    BufferRing() = default;
    BufferRing(std::unique_ptr<BufferRingImpl> impl) { reset(std::move(impl)); }
    BufferRing(BufferRing&& other) : m_impl(std::move(other.m_impl)) {}

    void reset(std::unique_ptr<BufferRingImpl> impl)
    {
        assert(!mapped());
        assert(impl->itemSizeInBytes() == sizeof(T));
        m_impl = std::move(impl);
    }

    size_t totalSizeInBytes() const
    {
        return m_impl ? kBufferRingSize * m_impl->capacity() * m_impl->itemSizeInBytes() : 0;
    }

    size_t capacity() const { return m_impl->capacity(); }

    // Maps the next buffer in the ring, if one is not already mapped.
    RIVE_ALWAYS_INLINE void ensureMapped()
    {
        if (!mapped())
        {
            m_mappedMemory = m_nextMappedItem = reinterpret_cast<T*>(m_impl->mapBuffer());
            m_mappingEnd = m_mappedMemory + m_impl->capacity();
        }
    }

    const BufferRingImpl* impl() const { return m_impl.get(); }
    BufferRingImpl* impl() { return m_impl.get(); }

    // Is a buffer not mapped, or, has nothing been pushed yet to the currently-mapped buffer?
    size_t empty() const
    {
        assert(!m_mappedMemory == !m_nextMappedItem);
        return m_mappedMemory == m_nextMappedItem;
    }

    // How many bytes have been written to the currently-mapped buffer?
    // (Returns 0 if no buffer is mapped.)
    size_t bytesWritten() const
    {
        assert(!m_mappedMemory == !m_mappingEnd);
        return reinterpret_cast<uintptr_t>(m_nextMappedItem) -
               reinterpret_cast<uintptr_t>(m_mappedMemory);
    }

    // Is a buffer currently mapped?
    bool mapped() const
    {
        assert(!m_mappedMemory == !m_nextMappedItem && !m_mappedMemory == !m_mappingEnd);
        return m_mappedMemory != nullptr;
    }

    // Is there room to push() itemCount items to the currently-mapped buffer?
    bool hasRoomFor(size_t itemCount)
    {
        assert(mapped());
        return m_nextMappedItem + itemCount <= m_mappingEnd;
    }

    // Append and write a new item to the currently-mapped buffer. In order to enforce the
    // write-only requirement of a mapped buffer, this method does not return any pointers to the
    // client.
    template <typename... Args> RIVE_ALWAYS_INLINE void emplace_back(Args&&... args)
    {
        push() = {std::forward<Args>(args)...};
    }
    template <typename... Args> RIVE_ALWAYS_INLINE void set_back(Args&&... args)
    {
        push().set(std::forward<Args>(args)...);
    }

    // Called after all the data for a frame has been push()-ed to the mapped buffer. Unmaps and
    // submits the currently-mapped buffer (if any) for GPU rendering, in whatever way that is
    // meaningful for the PLSRenderContext implementation.
    void submit()
    {
        if (mapped())
        {
            m_impl->unmapAndSubmitBuffer(bytesWritten());
            m_mappingEnd = m_nextMappedItem = m_mappedMemory = nullptr;
        }
        assert(!mapped());
    }

private:
    template <typename... Args> RIVE_ALWAYS_INLINE T& push()
    {
        assert(mapped());
        assert(hasRoomFor(1));
        return *m_nextMappedItem++;
    }

    std::unique_ptr<BufferRingImpl> m_impl;
    T* m_mappedMemory = nullptr;
    T* m_nextMappedItem = nullptr;
    const T* m_mappingEnd = nullptr;
};
} // namespace rive::pls
